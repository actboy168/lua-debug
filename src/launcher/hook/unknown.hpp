#pragma once

#include <gumpp.hpp>

#include "hook_common.h"
#include "../symbol_resolver/symbol_resolver.h"
#include "../lua_delayload.h"

namespace autoattach {

#define FILED_VAR(name, ...) name##_t name;

    void attach_lua_vm(lua::state L);
    struct vmhooker {
        lua::hook origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;
        void call_origin_hook(lua::state L, lua::debug ar);
        void call_lua_sethook(lua::state L, lua::hook fn);
        bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver);
    };
}