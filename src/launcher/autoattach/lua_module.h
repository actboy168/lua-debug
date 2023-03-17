#pragma once

#include <autoattach/autoattach.h>
#include <resolver/lua_resolver.h>

#include <string>

namespace luadebug::autoattach {
    struct watchdog;
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
        size_t memory_size   = 0;
        lua_version version  = lua_version::unknown;
        lua_resolver resolver;
        watchdog* watchdog = nullptr;

        bool initialize(fn_attach attach_lua_vm);
    };
}
