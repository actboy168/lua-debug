#include <autoattach/ctx.h>
#include <autoattach/lua_module.h>
#include <bee/nonstd/unreachable.h>
#include <hook/watchdog.h>
#include <util/log.h>

#include <string>
#include <string_view>
#include <vector>

namespace luadebug::autoattach {
    static std::vector<watch_point> get_watch_points(work_mode mode, lua_version version) {
        if (mode == work_mode::launch) {
            return {
                { watch_point::type::ret, "lua_newstate" }
            };
        }
        switch (version) {
        case lua_version::luajit:
            return {
                { watch_point::type::common, "lj_dispatch_ins" },
                { watch_point::type::common, "lj_dispatch_call" },
                { watch_point::type::common, "lj_vm_call" },
                { watch_point::type::common, "lj_vm_pcall" },
                { watch_point::type::common, "lj_vm_cpcall" },
                { watch_point::type::common, "lj_vm_resume" },
                { watch_point::type::common, "lj_meta_tget" },
                { watch_point::type::luajit_global, "lj_dispatch_update" },
                { watch_point::type::luajit_jit, "lj_dispatch_stitch" },
            };
        case lua_version::lua51:
            return {
                { watch_point::type::common, "luaD_precall" },
                { watch_point::type::common, "luaV_gettable" },
                { watch_point::type::common, "luaV_settable" },
            };
        case lua_version::lua54:
            return {
                { watch_point::type::common, "luaD_poscall" },
                { watch_point::type::common, "luaH_finishset" },
                { watch_point::type::common, "luaV_finishget" },
            };
        default:
            return {
                { watch_point::type::common, "lua_settop" },
                { watch_point::type::common, "luaL_openlibs" },
                { watch_point::type::common, "lua_newthread" },
                { watch_point::type::common, "lua_close" },
                { watch_point::type::common, "lua_type" },
                { watch_point::type::common, "lua_pushnil" },
                { watch_point::type::common, "lua_pushnumber" },
                { watch_point::type::common, "lua_pushlstring" },
                { watch_point::type::common, "lua_pushboolean" },
            };
        }
    }

    watchdog* create_watchdog(work_mode mode, lua_version version, const lua::resolver& resolver) {
        auto context = std::make_unique<watchdog>();
        if (!context->init()) {
            return nullptr;
        }
        if (context->init_watch(resolver, get_watch_points(mode, version))) {
            // TODO: fix other thread pc
            context->hook();
            return context.release();
        }
        if (version == lua_version::unknown) {
            // TODO: more errmsg
            log::fatal("watchdog initialize failed");
            return nullptr;
        }
        if (context->init_watch(resolver, get_watch_points(mode, lua_version::unknown))) {
            // TODO: fix other thread pc
            context->hook();
            return context.release();
        }
        // TODO: more errmsg
        log::fatal("watchdog initialize failed");
        return nullptr;
    }
}
