#pragma once

#include <resolver/lua_delayload.h>

#include <string_view>

#include <gumpp.hpp>

namespace luadebug {
    struct lua_resolver : lua::resolver {
        intptr_t find(std::string_view name) const override;
        intptr_t find_export(std::string_view name) const;
        intptr_t find_symbol(std::string_view name) const;
        std::string_view module_name;
        mutable Gum::Module* luamodule = nullptr;
    };
}