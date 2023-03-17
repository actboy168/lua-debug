#pragma once

#include <resolver/lua_delayload.h>

#include <string_view>

namespace luadebug::autoattach {
    struct watch_point {
        enum class type {
            common,
            luajit_global,
            luajit_jit,
        };
        type listener;
        std::string_view funcname;
        void* address = nullptr;
        explicit operator bool() const;
        bool find_symbol(const lua::resolver& resolver);
    };
}
