#include "unknown.hpp"
#include <vector>
#include <string>
#include <atomic>
namespace autoattach {
    struct watch_point {
        std::string funcname;
        void *address;
    };

    struct vmhook_template : vmhook {
		vmhook_template() = default;
		vmhook_template(const vmhook_template&) = delete;
        std::vector <watch_point> wather_points;
		vmhooker hooker;
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
			if (!hooker.get_symbols(resolver)) {
				return false;
			}
            for (auto &&watch: wather_points) {
                watch.address = resolver->getsymbol(watch.funcname.c_str());
            }
            for (auto &&watch: wather_points) {
                if (watch.address)
                    return true;
            }
            return false;
        }

        static void attach_lua_Hooker(lua_State *L, lua_Debug *ar) {
            attach_lua_vm(L);
            auto &_self = get_this();
            _self.hooker.attach_lua_Hooker(L, ar);
            //inject success disable hook
            _self.unhook();
        }

        void call_lua_sethook(lua_State *L) {
            hooker.call_lua_sethook(L, attach_lua_Hooker);
        }

        static void default_watch(void *address, DobbyRegisterContext *ctx) {
            auto L = (lua_State *) getfirstarg(ctx);
            auto &_self = get_this();
            //make sure no stackoverflow
            thread_local std::atomic_bool inwatch = false;
			bool test = false;
            if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
                _self.call_lua_sethook(L);
				inwatch.store(false, std::memory_order_release);
            }
        }

        static vmhook_template &get_this();
    };
}