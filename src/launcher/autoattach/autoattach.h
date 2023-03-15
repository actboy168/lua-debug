#pragma once

#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {
    enum class attach_status {
        success,
        fatal,
        wait,
    };
    typedef attach_status (*fn_attach)(lua::state L);
    void initialize(fn_attach attach, bool ap);
}
