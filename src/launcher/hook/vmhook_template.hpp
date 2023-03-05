#pragma once

#include <hook/unknown.hpp>
#include <hook/hook_common.h>
#include <hook/watch_point.h>
#include <hook/listener.h>
#include <resolver/lua_delayload.h>

#include <gumpp.hpp>
#include <vector>
#include <string>
#include <atomic>

namespace luadebug::autoattach {

    struct vmhook_template: vmhook {
        static_assert(std::is_same_v<lua::state, uintptr_t>);
        
        vmhook_template(Gum::RefPtr<Gum::Interceptor> in);
        vmhook_template(const vmhook_template&) = delete;
        bool hook() override;
        void unhook() override;
        bool get_symbols(const lua::resolver& resolver) override;
        void watch_entry(lua::state L);
        bool hook(const watch_point& point);

        std::vector<watch_point> wather_points;
        vmhooker hooker;
        std::atomic_bool inwatch = false;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        common_listener        listener_common;
        luajit_global_listener listener_luajit_global;
        luajit_jit_listener    listener_luajit_jit;
    };
}