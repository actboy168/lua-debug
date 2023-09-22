#pragma once

#include <autoattach/autoattach.h>
#include <autoattach/lua_version.h>
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
        lua_resolver resolver;
        work_mode mode;
        struct watchdog* watchdog = nullptr;
        std::string debugger_path;

        lua_module(work_mode mode)
            : mode(mode) {}
        lua_module(const lua_module&)            = delete;
        lua_module& operator=(const lua_module&) = delete;

        lua_module(lua_module&& rhs) {
            swap(std::forward<lua_module>(rhs));
        };
        lua_module& operator=(lua_module&& rhs) {
            swap(std::forward<lua_module>(rhs));
            return *this;
        };

        void swap(lua_module&& rhs) {
            std::swap(path, rhs.path);
            std::swap(name, rhs.name);
            std::swap(memory_address, rhs.memory_address);
            std::swap(memory_size, rhs.memory_size);
            std::swap(version, rhs.version);
            std::swap(resolver, rhs.resolver);
            std::swap(mode, rhs.mode);
            std::swap(watchdog, rhs.watchdog);
        }

        bool initialize();
        ~lua_module();
    };
}
