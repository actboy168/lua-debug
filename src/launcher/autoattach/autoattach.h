#pragma once

#include <autoattach/lua_version.h>
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
    attach_status attach_lua(lua::state L);
    void initialize(work_mode mode);
}
