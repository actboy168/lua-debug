#pragma once

#include <gumpp.hpp>

#include <hook/hook_common.h>
#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {

#define FILED_VAR(name, ...) name##_t name;

    struct vmhooker {
        lua::hook origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;
        void call_origin_hook(lua::state L, lua::debug ar);
        void reset_lua_hook(lua::state L);
        void call_lua_sethook(lua::state L, lua::hook fn);
    };
}