#pragma once

#include <Windows.h>

struct lua_State;

namespace autoattach {
	typedef void(*fn_attach)(lua_State* L);
	void    initialize(fn_attach attach, bool ap);
	FARPROC luaapi(const char* name);
}
