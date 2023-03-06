#pragma once

#include <autoattach/lua_module.h>
#include <memory>

namespace luadebug::lua {
    struct resolver;
}

namespace luadebug::autoattach {
    struct watchdog;
    watchdog* create_watchdog(lua_version v, const lua::resolver& resolver);
}
