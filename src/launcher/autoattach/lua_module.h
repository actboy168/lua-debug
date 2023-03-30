#pragma once

#include <autoattach/autoattach.h>
#include <config/config.h>
#include <resolver/lua_resolver.h>

#include <string>

namespace luadebug::autoattach {
    struct watchdog;
    struct lua_module {
        std::string path;
        std::string name;
        void* memory_address = 0;
        size_t memory_size   = 0;
        lua_version version  = lua_version::unknown;
        std::unique_ptr<lua::resolver> resolver;
        struct watchdog* watchdog = nullptr;
        config::Config config;

        bool initialize(fn_attach attach_lua_vm);
    };
}
