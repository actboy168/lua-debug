#pragma once

#include <resolver/lua_delayload.h>

#include <string>
#include <string_view>

namespace luadebug {
    struct lua_resolver : lua::resolver {
        lua_resolver(std::string_view name);
        ~lua_resolver() = default;
        intptr_t find(std::string_view name) const override;
        intptr_t find_export(std::string_view name) const;
        intptr_t find_symbol(std::string_view name) const;
        std::string module_name;
    };
}