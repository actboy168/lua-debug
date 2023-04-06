#pragma once

#include <autoattach/autoattach.h>
#include <autoattach/lua_module.h>

#include <memory>

namespace luadebug::lua {
    struct resolver;
}

namespace luadebug::autoattach {
    struct watchdog;
    watchdog* create_watchdog(work_mode mode, lua_version v, const lua::resolver& resolver);
}
