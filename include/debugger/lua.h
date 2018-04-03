#pragma once

#if defined(DEBUGGER_BRIDGE)
#include <debugger/bridge/luacompatibility.h>
#else
#include <lua.hpp>

namespace lua {
	struct Debug : public lua_Debug { };
}

#endif
