#include "unknown.hpp"
#include <vector>
#include <string>
#include <atomic>
namespace autoattach {
    struct watch_point {
        std::string funcname;
        void *address;
    };

    struct vmhook_template : unknown_vmhook {
        std::vector <watch_point> wather_points;

        bool hook() override {
            for (auto &&watch: wather_points) {
                if (watch.address)
                    DobbyInstrument(watch.address, default_watch);
            }
			return true;
        }

        void unhook() override {
            for (auto &&watch: wather_points) {
                if (watch.address) {
                    DobbyDestroy(watch.address);
                }
            }
        }

        bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver) override {
            for (auto &&watch: wather_points) {
                watch.address = resolver->getsymbol(watch.funcname.c_str());
            }
            for (auto &&watch: wather_points) {
                if (watch.address)
                    return true;
            }
            return false;
        }

        static void attach_lua_Hook(lua_State *L, lua_Debug *ar) {
            attach_lua_vm(L);
            unknown_vmhook &_self = get_this();
            _self.attach_lua_Hook(L, ar);
            //inject success disable hook
            _self.unhook();
        }

        void call_lua_hook(lua_State *L) {
            unknown_vmhook::call_lua_hook(L, attach_lua_Hook);
        }

        static void default_watch(void *address, DobbyRegisterContext *ctx) {
            auto L = (lua_State *) getfirstarg(ctx);
            vmhook_template &_self = get_this();
            //make sure no stackoverflow
            thread_local std::atomic_bool inwatch = false;
            if (!inwatch.load(std::memory_order_acquire)) {
				inwatch.store(true, std::memory_order_release);
                _self.call_lua_hook(L);
				inwatch.store(false, std::memory_order_release);
            }
        }

        static vmhook_template &get_this();
    };
}