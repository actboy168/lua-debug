#include <hook/watchdog.h>
#include <resolver/lua_delayload.h>
#include <autoattach/autoattach.h>
#include <util/log.h>

#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>

namespace luadebug::autoattach {
    watchdog::watchdog() 
        : interceptor{Gum::Interceptor_obtain()}
    {}

    bool watchdog::hook() {
        for (auto& point: watch_points) {
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

    void watchdog::unhook() {
        interceptor->detach(&listener_common);
        interceptor->detach(&listener_luajit_global);
        interceptor->detach(&listener_luajit_jit);
    }

    bool watchdog::init(const lua::resolver& resolver, std::vector<watch_point>&& points) {
        bool ok = false;
        for (auto& point: points) {
            if (point.find_symbol(resolver)) {
                ok = true;
            }
        }
        if (ok) {
            watch_points = std::move(points);
        }
        return ok;
    }

    attach_status attach_lua_vm(lua::state L);
    static watchdog* g_watchdog = nullptr; //TODO: dont use global
    static void hook_attach_lua(lua::state L, lua::debug ar) {
        if (!g_watchdog)
            return; //TODO: maybe throw an exception
        g_watchdog->attach_lua(L, ar);
    }

    void watchdog::attach_lua(lua::state L, lua::debug ar) {
        reset_luahook(L, ar);
        switch (attach_lua_vm(L)) {
        case attach_status::fatal:
        case attach_status::success:
            //inject success disable hook
            unhook();
            //TODO: how to free so
            //TODO: free all resources
            break;
        case attach_status::wait:
            set_luahook(L);
            break;
        default:
            std::unreachable();
        }
    }

    void watchdog::reset_luahook(lua::state L, lua::debug ar) {
        if (origin_hook) {
            origin_hook(L, ar);
        }
        lua::call<lua_sethook>(L, origin_hook, origin_hookmask, origin_hookcount);
    }

    void watchdog::set_luahook(lua::state L) {
        origin_hook = lua::call<lua_gethook>(L);
        origin_hookmask = lua::call<lua_gethookmask>(L);
        origin_hookcount = lua::call<lua_gethookcount>(L);
        lua::call<lua_sethook>(L, hook_attach_lua, 0 | 1 | 2, 0);
    }

    void watchdog::watch_entry(lua::state L) {
        bool test = false;
        if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            g_watchdog = this;
            set_luahook(L);
        }
    }
}
