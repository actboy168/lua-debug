#pragma once

#if defined(DEBUGGER_BRIDGE)
#include "bridge/dbg_luacompatibility.h"
#else
#include <lua.hpp>

namespace lua {
	struct Debug : public lua_Debug { };
}

#endif
