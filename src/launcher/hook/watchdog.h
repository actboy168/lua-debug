#pragma once

#include <hook/watch_point.h>
#include <hook/listener.h>
#include <resolver/lua_delayload.h>

#include <gumpp.hpp>
#include <vector>
#include <string>
#include <atomic>

namespace luadebug::autoattach {
    struct watchdog {
        static_assert(std::is_same_v<lua::state, uintptr_t>);
    public:
        watchdog();
        watchdog(const watchdog&) = delete;
        bool init(const lua::resolver& resolver, std::vector<watch_point>&& points);
        bool hook();
        void unhook();
        void watch_entry(lua::state L);
        void attach_lua(lua::state L, lua::debug ar);
    private:
        void reset_luahook(lua::state L, lua::debug ar);
        void set_luahook(lua::state L);
    private:
        std::vector<watch_point>      watch_points;
        std::atomic_bool              inwatch = false;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        common_listener               listener_common;
        luajit_global_listener        listener_luajit_global;
        luajit_jit_listener           listener_luajit_jit;
        lua::hook                     origin_hook;
        int                           origin_hookmask;
        int                           origin_hookcount;
    };
}
