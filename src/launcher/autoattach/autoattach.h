#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {
	typedef int(*fn_attach)(lua::state L);
	void    initialize(fn_attach attach, bool ap);
}
