#include <Windows.h>
#include <base/hook/iat.h>
#include <base/hook/eat.h>
#include <base/hook/fp_call.h>
#include <base/win/process.h>
#include "utility.h"

#pragma data_seg("Shared")
bool                         shared_enable = false;
uint16_t                     shared_port   = 0;
wchar_t                      shared_luadll_str[MAX_PATH] = {0};
size_t                       shared_luadll_len = 0;
#pragma data_seg()
#pragma comment(linker, "/section:Shared,rws")

std::wstring_view shared_luadll(shared_luadll_str, shared_luadll_len);

void initialize_debugger(void* L)
{
	if (GetModuleHandleW(L"debugger.dll")) {
		return;
	}
	fs::path debugger = get_self_path().remove_filename() / L"debugger.dll";
	HMODULE dll = LoadLibraryW(debugger.c_str());
	if (!dll) {
		return;
	}
	uintptr_t set_luadll   = (uintptr_t)GetProcAddress(dll, "set_luadll");
	uintptr_t start_server = (uintptr_t)GetProcAddress(dll, "start_server");
	uintptr_t attach_lua   = (uintptr_t)GetProcAddress(dll, "attach_lua");
	if (!set_luadll || !start_server || !attach_lua) {
		return;
	}
	if (shared_luadll.size() != 0) {
		base::c_call<void>(set_luadll, shared_luadll.data(), shared_luadll.size());
	}
	shared_port = base::c_call<uint16_t>(start_server, "127.0.0.1", 0, true);
	base::c_call<void>(attach_lua, L, true);
}

void uninitialize_debugger(void* L)
{
	HMODULE dll = GetModuleHandleW(L"debugger.dll");
	if (!dll) {
		return;
	}
	uintptr_t detach_lua = (uintptr_t)GetProcAddress(dll, "detach_lua");
	if (!detach_lua) {
		return;
	}
	base::c_call<void>(detach_lua, L);
	Sleep(1000);
}

struct lua_newstate
	: public hook_helper<lua_newstate>
{
	static void* __cdecl fake(void* f, void* ud)
	{
		void* L = base::c_call<void*>(real, f, ud);
		if (L) initialize_debugger(L);
		return L;
	}
};

struct luaL_newstate
	: public hook_helper<luaL_newstate>
{
	static void* __cdecl fake()
	{
		void* L = base::c_call<void*>(real);

		if (L) initialize_debugger(L);
		return L;
	}
};

struct lua_close
	: public hook_helper<lua_close>
{
	static void __cdecl fake(void* L)
	{
		uninitialize_debugger(L);
		return base::c_call<void>(real, L);
	}
};

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		::DisableThreadLibraryCalls(module);
		if (shared_enable) {
			if (shared_luadll.size() != 0) {
				lua_newstate::init(shared_luadll.data(), "lua_newstate");
				luaL_newstate::init(shared_luadll.data(), "luaL_newstate");
				lua_close::init(shared_luadll.data(), "lua_close");
			}
			else {
				const char* luadll = search_api("lua_newstate", "luaL_newstate");
				if (luadll) {
					lua_newstate::init(luadll, "lua_newstate");
					luaL_newstate::init(luadll, "luaL_newstate");
					lua_close::init(luadll, "lua_close");
				}
				else {
					lua_newstate::init(L"luacore.dll", "lua_newstate");
					luaL_newstate::init(L"luacore.dll", "luaL_newstate");
					lua_close::init(L"luacore.dll", "lua_close");
				}
			}
		}
	}
	return TRUE;
}

extern "C" __declspec(dllexport) 
bool set_luadll(const wchar_t* str, size_t len)
{
	if (len + 1 > _countof(shared_luadll_str)) { return false; }
	memcpy(shared_luadll_str, str, len * sizeof(wchar_t));
	shared_luadll_str[len] = L'\0';
	shared_luadll_len = len;
	return true;
}

extern "C" __declspec(dllexport)
uint16_t create_process_with_debugger(const wchar_t* application, const wchar_t* command_line, const wchar_t* current_directory)
{
	shared_port = 0;
	shared_enable = true;

	base::win::process p;
	p.inject(get_self_path());
	if (!p.create(application, command_line, current_directory)) {
		return 0;
	}
	for (int i = 0; i < 1000 || !p.is_running(); ++i)
	{
		if (shared_port != 0)
			return shared_port;
		Sleep(100);
	}
	return 0;
}
