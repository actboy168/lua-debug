#include <autoattach/autoattach.h>
#include <autoattach/luaversion.h>
#include <autoattach/wait_dll.h>
#include <util/common.hpp>
#include <util/log.h>
#include <hook/hook_common.h>
#include <resolver/lua_resolver.h>

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <gumpp.hpp>

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
        if (is_lua_module(path.c_str())){
            // find lua module lazy 
            std::thread(start).detach();
            return true;
        }
        return false;
    }

    int attach_lua_vm(lua::state L) {
        return debuggerAttach(L);
    }

    static RuntimeModule to_runtim_module(const Gum::ModuleDetails& details){
        RuntimeModule rm = {};
        const char* path = details.path();
        const char* name = details.name();
        auto range = details.range();
        strcpy(rm.path, path);
        strcpy(rm.name, name);
        rm.load_address = range.base_address;
        rm.size = range.size;
        return rm;
    }

    void start() {
        RuntimeModule rm = {};
        Gum::Process::enumerate_modules([&rm](const Gum::ModuleDetails& details)->bool{
            if (is_lua_module(details.path())) {
                rm = to_runtim_module(details);
                return false;
            }
            return true;
        });
        if (!rm.load_address) {
            if (!wait_dll(load_lua_module))
                log::fatal("can't find lua module");
            return;
        }

        log::info(std::format("find lua module path:{}", rm.path).c_str());
        
        lua_resolver r(rm.path);
        auto error_msg = lua::initialize(r);
        if (error_msg) {
            log::fatal(std::format("lua::initialize failed, can't find {}", error_msg).c_str());
            return;
        }

        auto luaversion = get_lua_version(rm);

        log::info(std::format("current lua version: {}", lua_version_to_string(luaversion)).c_str());

        auto vmhook = create_vmhook(luaversion);
        if (!vmhook->get_symbols(r)) {
           log::fatal("get_symbols failed");
           return;
        }
        //TODO: fix other thread pc
        vmhook->hook();
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
