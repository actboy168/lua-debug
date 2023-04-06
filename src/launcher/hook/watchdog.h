#pragma once

#include <autoattach/autoattach.h>
#include <hook/listener.h>
#include <hook/watch_point.h>
#include <resolver/lua_delayload.h>

#include <gumpp.hpp>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace luadebug::autoattach {
    struct watchdog {
        static_assert(std::is_same_v<lua::state, uintptr_t>);

    public:
        watchdog();
        ~watchdog();
        watchdog(const watchdog&) = delete;
        bool init();
        bool init_watch(const lua::resolver& resolver, std::vector<watch_point>&& points);
        bool hook();
        void unhook();
        void watch_entry(lua::state L);
        void attach_lua(lua::state L, lua::debug ar);

    private:
        std::mutex mtx;
        std::vector<watch_point> watch_points;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        common_listener listener_common;
        luajit_global_listener listener_luajit_global;
        luajit_jit_listener listener_luajit_jit;
        ret_listener listener_ret;
        std::set<lua::state> lua_state_hooked;
        uint8_t luahook_index;
        lua::cfunction luahook_set = nullptr;
    };
}
