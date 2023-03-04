#pragma once

#include <hook/unknown.hpp>
#include <hook/hook_common.h>
#include <hook/vm_watcher.hpp>
#include <resolver/lua_delayload.h>

#include <gumpp.hpp>
#include <vector>
#include <string>
#include <atomic>

namespace luadebug::autoattach {
    struct watch_point {
        std::string_view funcname;
        void *address;
    };

    void get_watch_symbol(watch_point& watch, const lua::resolver& resolver);

    struct vmhook_template: vmhook, Gum::NoLeaveInvocationListener, vm_watcher {
        static_assert(std::is_same_v<lua::state, vm_watcher::value_t>);
        
        vmhook_template(Gum::RefPtr<Gum::Interceptor> in);
        vmhook_template(const vmhook_template&) = delete;
        bool hook() override;
        void unhook() override;
        bool get_symbols(const lua::resolver& resolver) override;
        void watch_entry(lua::state L) override;
        void on_enter(Gum::InvocationContext * context) override;

        std::vector<watch_point> wather_points;
        vmhooker hooker;
        std::atomic_bool inwatch = false;
        Gum::RefPtr<Gum::Interceptor> interceptor;
    };
}