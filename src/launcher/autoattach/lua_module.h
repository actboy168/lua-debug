#pragma once

#include <autoattach/autoattach.h>
#include <resolver/lua_resolver.h>
#include <string_view>

namespace luadebug::autoattach {
    enum class lua_version {
        unknown,
        luajit,
        lua51,
        lua52,
        lua53,
        lua54,
    };
    struct lua_module {
        std::string path;
        std::string name;
        void* memory_address = 0;
        size_t memory_size = 0;
        lua_version version = lua_version::unknown;
        lua_resolver resolver;

        bool initialize();
    };
}
