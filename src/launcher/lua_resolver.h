#pragma once

#include "lua_delayload.h"
#include <string_view>

namespace lua_delayload {
    struct lua_resolver : resolver {
        lua_resolver(const std::string_view& module_name);
        intptr_t find(const char* name) const override;
        intptr_t find_export(const char* name) const;
        intptr_t find_symbol(const char* name) const;
        std::string_view module_name;
    };
}