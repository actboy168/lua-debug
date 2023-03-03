#pragma once

#include <string_view>
#include <autoattach/autoattach.h>

namespace luadebug::autoattach {
    enum class lua_version {
        unknown,
        luajit,
        lua51,
        lua52,
        lua53,
        lua54,
    };
    lua_version lua_version_from_string(const std::string_view& v);
    const char* lua_version_to_string(lua_version v);
    lua_version get_lua_version(const RuntimeModule& rm);
}
