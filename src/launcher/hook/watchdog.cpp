#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <hook/watchdog.h>
#include <resolver/lua_delayload.h>
#include <util/log.h>

#include <atomic>
#include <cassert>

namespace luadebug::autoattach {
    template <typename T>
    constexpr int countl_zero(T v) noexcept {
        T y            = 0;
        unsigned int n = std::numeric_limits<T>::digits;
        unsigned int c = std::numeric_limits<T>::digits / 2;
        do {
            y = static_cast<T>(v >> c);
            if (y != 0) {
                n -= c;
                v = y;
            }
            c >>= 1;
        } while (c != 0);
        return static_cast<int>(n) - static_cast<int>(v);
    }
    template <typename T>
    constexpr int countr_zero(T v) noexcept {
        constexpr int digits = std::numeric_limits<T>::digits;
        return digits - countl_zero(static_cast<T>(static_cast<T>(~v) & static_cast<T>(v - 1)));
    }

    template <typename T>
    constexpr int countr_one(T v) noexcept {
        return countr_zero(static_cast<T>(~v));
    }

    struct trampoline {
        template <size_t Index>
        struct callback {
            static inline watchdog* w = nullptr;
            static inline lua::hook origin_hook;
            static inline int origin_hookmask;
            static inline int origin_hookcount;
            static void attach_lua(lua::state L, lua::debug ar) {
                reset_luahook(L, ar);
                if (w) {
                    w->attach_lua(L, ar);
                }
            }
            static lua::cfunction create(watchdog* _w) {
                assert(w == nullptr);
                w = _w;
                return set_luahook;
            }
            static void destroy(watchdog* _w) {
                assert(w == _w);
                w = nullptr;
            }
            static void reset_luahook(lua::state L, lua::debug ar) {
                if (origin_hook) {
                    origin_hook(L, ar);
                }
                lua::call<lua_sethook>(L, origin_hook, origin_hookmask, origin_hookcount);
            }
            static int set_luahook(lua::state L) {
                origin_hook      = lua::call<lua_gethook>(L);
                origin_hookmask  = lua::call<lua_gethookmask>(L);
                origin_hookcount = lua::call<lua_gethookcount>(L);
                lua::call<lua_sethook>(L, attach_lua, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
                return 0;
            }
        };
        static inline std::atomic<uint8_t> used = 0;
        static constexpr uint8_t limit          = 0x3;
        static uint8_t create_instance_id() {
            for (;;) {
                uint8_t mark = used;
                uint8_t n    = (uint8_t)countr_one(mark);
                if (n >= limit) {
                    return limit;
                }
                if (used.compare_exchange_weak(mark, mark | (1 << n))) {
                    return n;
                }
            }
        }
        static void destroy_instance_id(uint8_t n) {
            for (;;) {
                uint8_t mark = used;
                if (!(mark & (1 << n))) {
                    return;
                }
                if (used.compare_exchange_weak(mark, mark & (~(1 << n)))) {
                    return;
                }
            }
        }
        static std::tuple<uint8_t, lua::cfunction> create(watchdog* w) {
            uint8_t id = create_instance_id();
            if (id >= limit) {
                return { limit, nullptr };
            }
            auto fn = [w](size_t id) -> lua::cfunction {
                switch (id) {
                case 0x0:
                    return callback<0x0>::create(w);
                case 0x1:
                    return callback<0x1>::create(w);
                case 0x2:
                    return callback<0x2>::create(w);
                default:
                    std::unreachable();
                }
            };
            return { id, fn(id) };
        }
        static void destroy(watchdog* w, uint8_t index) {
            switch (index) {
            case 0x0:
                callback<0x0>::destroy(w);
                break;
            case 0x1:
                callback<0x1>::destroy(w);
                break;
            case 0x2:
                callback<0x2>::destroy(w);
                break;
            default:
                break;
            }
            destroy_instance_id(index);
        }
    };

    watchdog::watchdog(struct lua_module& lua_module)
        : lua_module(lua_module)
        , interceptor { Gum::Interceptor_obtain() } {}
    watchdog::~watchdog() {
        if (luahook_set) {
            trampoline::destroy(this, luahook_index);
            unhook();
        }
    }

    bool watchdog::hook() {
        std::lock_guard guard(mtx);
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
            case watch_point::type::ret:
                ok = interceptor->attach(point.address, &listener_ret, this);
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
        interceptor->detach(&listener_ret);
    }

    bool watchdog::init() {
        auto [index, func] = trampoline::create(this);
        luahook_index      = index;
        luahook_set        = func;
        if (!func) {
            log::fatal("Too many watchdog instances.");
            return false;
        }
        return true;
    }

    bool watchdog::init_watch(const lua::resolver& resolver, std::vector<watch_point>&& points) {
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

    void watchdog::attach_lua(lua::state L, lua::debug ar) {
        switch (autoattach::attach_lua(L, lua_module)) {
        case attach_status::fatal:
        case attach_status::success:
            // TODO: how to free so
            // TODO: free all resources
            break;
        case attach_status::wait:
            luahook_set(L);
            break;
        default:
            std::unreachable();
        }
    }

    void watchdog::watch_entry(lua::state L) {
        std::lock_guard guard(mtx);
        if (lua_state_hooked.find(L) != lua_state_hooked.end())
            return;
        luahook_set(L);
        lua_state_hooked.emplace(L);
    }
}
