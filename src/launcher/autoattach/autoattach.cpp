#include <autoattach/ctx.h>
#include <autoattach/lua_module.h>
#include <autoattach/wait_dll.h>
#include <bee/nonstd/format.h>
#include <hook/watchdog.h>
#include <resolver/lua_resolver.h>
#include <util/log.h>

#include <array>
#include <atomic>
#include <gumpp.hpp>
#include <memory>
#include <string>
#include <thread>

namespace luadebug::autoattach {
    constexpr auto find_lua_module_key = "lua_newstate";
    constexpr auto lua_module_strings  = std::array<const char*, 3> {
        "luaJIT_BC_%s",  // luajit
        " $\n"
         "$Authors: "
         "R. Ierusalimschy, L. H. de Figueiredo & W. Celes"
         " $\n"
         "$URL: www.lua.org $\n",  // lua51
        "$LuaAuthors: "
         "R. Ierusalimschy, L. H. de Figueiredo, W. Celes"
         " $",  // others
    };
    static bool is_lua_module(const char* module_path, bool check_export = true, bool check_strings = false) {
        if (check_export && Gum::Process::module_find_export_by_name(module_path, find_lua_module_key)) return true;
        if (Gum::Process::module_find_symbol_by_name(module_path, find_lua_module_key)) return true;
        // TODO: when signature mode, check strings
        if (check_strings) {
            for (auto str : lua_module_strings) {
                if (!Gum::search_module_string(module_path, str).empty()) {
                    return true;
                }
            }
        }
        return false;
    }

    static void start();
    static bool load_lua_module(const std::string& path) {
        constexpr auto check_export =
#ifdef _WIN32
            true
#else
            false
#endif
            ;
        if (!is_lua_module(path.c_str(), check_export, true)) {
            return false;
        }
        // find lua module lazy
        std::thread(start).detach();
        return true;
    }

    void start() {
        auto ctx = ctx::get();
        std::unique_lock lock(ctx->mtx, std::try_to_lock);
        if (!lock.owns_lock()) {
            return;
        }

        if (ctx->lua_module) {
            // rehook
            ctx->lua_module->watchdog->hook();
            return;
        }

        bool found    = false;
        lua_module rm = {};
        Gum::Process::enumerate_modules([&rm, &found](const Gum::ModuleDetails& details) -> bool {
            if (is_lua_module(details.path())) {
                auto range        = details.range();
                rm.memory_address = range.base_address;
                rm.memory_size    = range.size;
                rm.path           = details.path();
                rm.name           = details.name();
                found             = true;
                return false;
            }
            return true;
        });
        if (!found) {
            if (!wait_dll(load_lua_module)) {
                log::fatal("can't find lua module");
            }
            return;
        }

        log::info("find lua module path:{}", rm.path);
        if (!rm.initialize(ctx::attach_lua_vm)) {
            return;
        }
        ctx->lua_module = rm;
    }

    void initialize(fn_attach attach, bool ap) {
        static std::atomic_bool injected;
        bool test = false;
        if (injected.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            log::init(ap);
            log::info("initialize");
            Gum::runtime_init();
            auto ctx            = ctx::get();
            ctx->debuggerAttach = attach;
            ctx->attach_process = ap;
            start();
            injected.store(false, std::memory_order_release);
        }
    }
}
