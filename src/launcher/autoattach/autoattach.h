#pragma once

#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {
    enum class attach_status {
        success,
        fatal,
        wait,
    };
    enum class work_mode {
        launch,
        attach,
    };
    struct lua_module;
    attach_status attach_lua(lua::state L, lua_module &module);
    void initialize(work_mode mode);
}
