#include <autoattach/autoattach.h>
#include <autoattach/lua_module.h>
#include <autoattach/wait_dll.h>
#include <bee/nonstd/format.h>
#include <resolver/lua_resolver.h>
#include <util/log.h>

#include <atomic>
#include <gumpp.hpp>
#include <memory>
#include <string>
#include <thread>

namespace luadebug::autoattach {
    fn_attach debuggerAttach;

    constexpr auto find_lua_module_key = "lua_newstate";
    static bool is_lua_module(const char* module_path) {
        if (Gum::Process::module_find_export_by_name(module_path, find_lua_module_key))
            return true;
        return Gum::Process::module_find_symbol_by_name(module_path, find_lua_module_key) != nullptr;
    }

    static void start();
    static bool load_lua_module(const std::string& path) {
        if (is_lua_module(path.c_str())) {
            // find lua module lazy
            std::thread(start).detach();
            return true;
        }
        return false;
    }

    attach_status attach_lua_vm(lua::state L) {
        return debuggerAttach(L);
    }

    void start() {
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
        if (!rm.initialize(attach_lua_vm)) {
            return;
        }
    }
    void initialize(fn_attach attach, bool ap) {
        static std::atomic_bool injected;
        bool test = false;
        if (injected.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            log::init(ap);
            log::info("initialize");
            Gum::runtime_init();
            debuggerAttach = attach;
            luadebug::autoattach::start();
            injected.store(false, std::memory_order_release);
        }
    }
}
