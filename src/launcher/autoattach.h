#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include "attach_args.h"
#endif

namespace autoattach {
#ifdef _WIN32
	typedef void(*fn_attach)(lua_State* L);
#else
	typedef void(*fn_attach)(lua_State* L, attach_args* args);
#endif
	void    initialize(fn_attach attach, bool ap);
#ifdef _WIN32
	FARPROC luaapi(const char* name);
#endif
}
