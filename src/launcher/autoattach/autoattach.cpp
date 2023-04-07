#include <autoattach/autoattach.h>
#include <autoattach/ctx.h>
#include <autoattach/session.h>
#include <autoattach/wait_dll.h>
#include <util/log.h>
#include <util/path.h>

#include <array>
#include <atomic>
#include <gumpp.hpp>
#include <string>
#include <thread>

namespace luadebug::autoattach {
#ifdef _WIN32
#    define EXT ".dll"
#else
#    define EXT ".so"
#endif
    constexpr auto lua_module_backlist = {
        "launcher" EXT,
        "luadebug" EXT,
    };

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

    static bool is_lua_module(session& session, const char* module_path, bool check_export, bool check_strings) {
        auto str = std::string_view(module_path);
        // blacklist module
        auto root = get_plugin_root();
        if (root) {
            if (str.find((*root).string()) != std::string_view::npos) {
                // in luadebug root dir
                for (auto& s : lua_module_backlist) {
                    if (str.find(s) != std::string_view::npos)
                        return false;
                }
            }
        }
        const auto& target_lua_module = session.lua_module;
        if (!target_lua_module.empty() && str.find(target_lua_module) != std::string_view::npos) return true;
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
        if (!is_lua_module(*ctx::get()->session, path.c_str(), check_export, true)) {
            return false;
        }
        // find lua module lazy
        std::thread(start).detach();
        return true;
    }

    static void start_impl(luadebug::autoattach::ctx* ctx) {
        if (ctx->lua_module) {
            return;
        }
        Gum::Process::enumerate_modules([&ctx](const Gum::ModuleDetails& details) -> bool {
            auto& session = *ctx->session;
            if (is_lua_module(session, details.path(), true, false)) {
                ctx->lua_module                 = lua_module { session.mode };
                auto range                      = details.range();
                ctx->lua_module->memory_address = range.base_address;
                ctx->lua_module->memory_size    = range.size;
                ctx->lua_module->path           = details.path();
                ctx->lua_module->name           = details.name();
                ctx->lua_module->version        = session.version;
                ctx->session.reset();
                return false;
            }
            return true;
        });
        if (!ctx->lua_module) {
            if (ctx->wait_dll)
                return;
            if (!wait_dll(load_lua_module)) {
                log::fatal("can't find lua module");
            }
            else {
                ctx->wait_dll = true;
            }
            return;
        }

        log::info("find lua module path:{}", ctx->lua_module->path);
        if (!ctx->lua_module->initialize()) {
            ctx->lua_module.reset();
            return;
        }
    }

    void start() {
        auto ctx = ctx::get();
        std::lock_guard guard(ctx->mtx);
        start_impl(ctx);
    }

    void start(work_mode mode) {
        auto ctx = ctx::get();
        std::lock_guard guard(ctx->mtx);
        ctx->session = init_session(mode);
        if (!ctx->session) {
            log::fatal("can't load config");
            return;
        }
        start_impl(ctx);
    }

    std::once_flag initialize_flag;
    void initialize(work_mode mode) {
        std::call_once(initialize_flag, []() {
            log::info("initialize");
            Gum::runtime_init();
        });
        start(mode);
    }
}
