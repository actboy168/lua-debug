#include <Windows.h>
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
	base::c_call<void>(start_server, "127.0.0.1", shared_port);
	base::c_call<void>(attach_lua, L, true);
}

struct lua_newstate
	: public eat_helper<lua_newstate>
{
	static void* __cdecl fake(void* f, void* ud)
	{
		void* L = base::c_call<void*>(real, f, ud);
		if (L) initialize_debugger(L);
		return L;
	}
};

struct luaL_newstate
	: public eat_helper<lua_newstate>
{
	static void* __cdecl fake()
	{
		void* L = base::c_call<void*>(real);
		if (L) initialize_debugger(L);
		return L;
	}
};

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		::DisableThreadLibraryCalls(module);
		if (shared_enable) {
			if (shared_luadll.size() == 0) {
				lua_newstate::init(L"luacore.dll", "lua_newstate");
				luaL_newstate::init(L"luacore.dll", "luaL_newstate");
			}
			else {
				lua_newstate::init(shared_luadll.data(), "lua_newstate");
				luaL_newstate::init(shared_luadll.data(), "luaL_newstate");
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
bool create_process_with_debugger(uint16_t port, const wchar_t* command_line, const wchar_t* current_directory)
{
	//TODO: ÒÆ³ý¶ÔportµÄÒÀÀµ
	shared_port = port;
	shared_enable = true;

	base::win::process p;
	p.inject(get_self_path());
	return p.create(std::optional<fs::path>(), command_line, current_directory);
}
