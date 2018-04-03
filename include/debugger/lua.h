#pragma once

#if defined(DEBUGGER_BRIDGE)
#include <debugger/bridge/lua.h>
#else
#include <lua.hpp>

namespace lua {
	struct Debug : public lua_Debug { };
}

#endif
