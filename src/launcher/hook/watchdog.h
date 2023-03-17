#pragma once

#include <autoattach/autoattach.h>
#include <hook/listener.h>
#include <hook/watch_point.h>
#include <resolver/lua_delayload.h>

#include <atomic>
#include <gumpp.hpp>
#include <string>
#include <vector>

namespace luadebug::autoattach {
    struct watchdog {
        static_assert(std::is_same_v<lua::state, uintptr_t>);

    public:
        watchdog(fn_attach attach_lua_vm);
        watchdog(const watchdog&) = delete;
        bool init(const lua::resolver& resolver, std::vector<watch_point>&& points);
        bool hook();
        void unhook();
        void watch_entry(lua::state L);
        void attach_lua(lua::state L, lua::debug ar, lua::hook fn);

    private:
        void reset_luahook(lua::state L, lua::debug ar);
        void set_luahook(lua::state L, lua::hook fn);

    private:
        std::vector<watch_point> watch_points;
        std::atomic_bool inwatch = false;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        common_listener listener_common;
        luajit_global_listener listener_luajit_global;
        luajit_jit_listener listener_luajit_jit;
        lua::hook origin_hook;
        int origin_hookmask;
        int origin_hookcount;
        fn_attach attach_lua_vm;
    };
}
