#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
struct lua_State;
namespace autoattach {
	typedef void(*fn_attach)(lua_State* L);
	void    initialize(fn_attach attach, bool ap);
#ifdef _WIN32
	FARPROC luaapi(const char* name);
#endif
}
