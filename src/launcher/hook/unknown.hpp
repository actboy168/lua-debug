#pragma once

#include <gumpp.hpp>

#include <hook/hook_common.h>
#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {

#define FILED_VAR(name, ...) name##_t name;

    void attach_lua_vm(lua::state L);
    struct vmhooker {
        lua::hook origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;
        void call_origin_hook(lua::state L, lua::debug ar);
        void call_lua_sethook(lua::state L, lua::hook fn);
    };
}