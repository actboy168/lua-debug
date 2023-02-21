#pragma once

#include "lua_delayload.h"
#include "symbol_resolver/symbol_resolver.h"

namespace lua_delayload {
    struct lua_resolver: resolver {
        lua_resolver(const RuntimeModule &module);
        intptr_t find(const char* name) override;
        std::unique_ptr<autoattach::symbol_resolver::interface> context;
    };
}