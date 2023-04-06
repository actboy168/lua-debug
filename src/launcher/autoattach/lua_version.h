#pragma once
#include <string_view>
namespace luadebug::autoattach {
    enum class lua_version {
        unknown,
        luajit,
        lua51,
        lua52,
        lua53,
        lua54,
        latest,
    };
    const char* lua_version_to_string(lua_version v);
    lua_version lua_version_from_string(const std::string_view& v);
}
