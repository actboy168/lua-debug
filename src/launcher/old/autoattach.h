#pragma once

#if defined(_WIN32)
#include <Windows.h>
#endif

struct lua_State;

namespace autoattach {
	typedef void(*fn_attach)(lua_State* L);
	void    initialize(fn_attach attach, bool ap);
#if defined(_WIN32)
	FARPROC luaapi(const char* name);
#endif
}
