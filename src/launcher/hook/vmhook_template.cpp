#include <hook/vmhook_template.hpp>

#include <hook/unknown.hpp>
#include <hook/hook_common.h>
#include <resolver/lua_delayload.h>
#include <util/log.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>

namespace luadebug::autoattach {
    void attach_lua_Hooker(lua::state L, lua::debug ar);

    vmhook_template::vmhook_template(Gum::RefPtr<Gum::Interceptor> in) 
        : interceptor{in}
    {}

    bool vmhook_template::hook() {
        for (auto& point: wather_points) {
            if (!point.address) {
                continue;
            }
            bool ok;
            switch (point.listener) {
            case watch_point::type::common:
                ok = interceptor->attach(point.address, &listener_common, this);
                break;
            case watch_point::type::luajit_global:
                ok = interceptor->attach(point.address, &listener_luajit_global, this);
                break;
            case watch_point::type::luajit_jit:
                ok = interceptor->attach(point.address, &listener_luajit_jit, this);
                break;
            default:
                std::unreachable();
            }
            if (!ok) {
                log::info("interceptor attach failed:{}[{}]", point.address, point.funcname);
            }
        }
        return true;
    }

    void vmhook_template::unhook() {
        interceptor->detach(&listener_common);
        interceptor->detach(&listener_luajit_global);
        interceptor->detach(&listener_luajit_jit);
    }

    bool vmhook_template::get_symbols(const lua::resolver& resolver) {
        for (auto& watch: wather_points) {
            watch.find_symbol(resolver);
        }
        for (auto& watch: wather_points) {
            if (watch.address)
                return true;
        }
        return false;
    }

    void vmhook_template::watch_entry(lua::state L) {
        bool test = false;
        if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            hooker.call_lua_sethook(L, attach_lua_Hooker);
        }
    }
}
