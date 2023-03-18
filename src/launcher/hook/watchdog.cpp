#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <hook/watchdog.h>
#include <resolver/lua_delayload.h>
#include <util/log.h>

namespace luadebug::autoattach {
    watchdog::watchdog(fn_attach attach_lua_vm)
        : interceptor { Gum::Interceptor_obtain() }
        , attach_lua_vm(attach_lua_vm) {}

    bool watchdog::hook() {
        for (auto& point : watch_points) {
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
        for (auto& point : points) {
            if (point.find_symbol(resolver)) {
                ok = true;
            }
        }
        if (ok) {
            watch_points = std::move(points);
        }
        return ok;
    }

    struct trampoline {
        template <size_t Index>
        struct callback {
            static inline watchdog* w;
            static void attach_lua(lua::state L, lua::debug ar) {
                if (w) {
                    w->attach_lua(L, ar, attach_lua);
                }
            }
            static lua::hook create(watchdog* _w) {
                w = _w;
                return attach_lua;
            }
        };
        static inline std::atomic<size_t> used = 0;
        static size_t create_instance_id() {
            // TODO: free instance id
            return 1 + std::atomic_fetch_add(&used, 1);
        }
        static lua::hook create(watchdog* w) {
            size_t id = create_instance_id();
            switch (id) {
            case 0x0:
                return callback<0x0>::create(w);
            case 0x1:
                return callback<0x1>::create(w);
            case 0x2:
                return callback<0x2>::create(w);
            case 0x3:
                return callback<0x3>::create(w);
            case 0x4:
                return callback<0x4>::create(w);
            case 0x5:
                return callback<0x5>::create(w);
            case 0x6:
                return callback<0x6>::create(w);
            case 0x7:
                return callback<0x7>::create(w);
            case 0x8:
                return callback<0x8>::create(w);
            case 0x9:
                return callback<0x9>::create(w);
            case 0xA:
                return callback<0xA>::create(w);
            case 0xB:
                return callback<0xB>::create(w);
            case 0xC:
                return callback<0xC>::create(w);
            case 0xD:
                return callback<0xD>::create(w);
            case 0xE:
                return callback<0xE>::create(w);
            case 0xF:
                return callback<0xF>::create(w);
            default:
                return 0;
            }
        }
    };

    void watchdog::attach_lua(lua::state L, lua::debug ar, lua::hook fn) {
        reset_luahook(L, ar);
        switch (attach_lua_vm(L)) {
        case attach_status::fatal:
        case attach_status::success:
            // TODO: how to free so
            // TODO: free all resources
            break;
        case attach_status::wait:
            set_luahook(L, fn);
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

    void watchdog::set_luahook(lua::state L, lua::hook fn) {
        origin_hook      = lua::call<lua_gethook>(L);
        origin_hookmask  = lua::call<lua_gethookmask>(L);
        origin_hookcount = lua::call<lua_gethookcount>(L);
        lua::call<lua_sethook>(L, fn, 0 | 1 | 2, 0);
    }

    void watchdog::watch_entry(lua::state L) {
        bool test = false;
        if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            unhook();
            lua::hook fn = trampoline::create(this);
            if (!fn) {
                log::fatal("Too many watchdog instances.");
                return;
            }
            set_luahook(L, fn);
        }
    }
}
