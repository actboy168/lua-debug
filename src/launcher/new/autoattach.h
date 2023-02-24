#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include "../lua_delayload.h"

namespace autoattach {
	typedef void(*fn_attach)(lua::state L);
	void    initialize(fn_attach attach, bool ap);
#ifdef _WIN32
	FARPROC luaapi(const char* name);
#endif
}
