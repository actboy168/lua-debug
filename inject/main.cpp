#include <Windows.h>
#include <base/hook/eat.h>
#include <base/hook/fp_call.h>
#include <base/win/process.h>
#include "utility.h"

#pragma data_seg("Shared")
cstring_t<wchar_t, MAX_PATH> shared_luadll = { };
uint16_t                     shared_port   = 0;
#pragma data_seg()
#pragma comment(linker, "/section:Shared,rws")

void initialize_debugger(void* L);

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

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		::DisableThreadLibraryCalls(module);
		lua_newstate::init(L"luacore.dll", "lua_newstate");
		luaL_newstate::init(L"luacore.dll", "luaL_newstate");
	}
	return TRUE;
}

extern "C" __declspec(dllexport) 
bool set_luadll(const wchar_t* str, size_t len)
{
	return shared_luadll.assign(str, len);
}

extern "C" __declspec(dllexport)
bool creeat_process_with_debugger(uint16_t port, const wchar_t* command_line, const wchar_t* current_directory)
{
	//TODO: ÒÆ³ý¶ÔportµÄÒÀÀµ
	shared_port = port;

	base::win::process p;
	p.inject(get_self_path());
	return p.create(std::optional<fs::path>(), command_line, current_directory);
}
