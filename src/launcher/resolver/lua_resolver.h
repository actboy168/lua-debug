#pragma once

#include <resolver/lua_delayload.h>
#include <string_view>

namespace luadebug {
    struct lua_resolver : lua::resolver {
        lua_resolver(const std::string_view& module_name);
        intptr_t find(const char* name) const override;
        intptr_t find_export(const char* name) const;
        intptr_t find_symbol(const char* name) const;
        std::string_view module_name;
    };
}