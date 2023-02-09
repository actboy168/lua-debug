#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include "attach_args.h"

namespace autoattach {
	typedef void(*fn_attach)(lua_State* L, attach_args* args);
	void    initialize(fn_attach attach, bool ap);
#ifdef _WIN32
	FARPROC luaapi(const char* name);
#endif
}
