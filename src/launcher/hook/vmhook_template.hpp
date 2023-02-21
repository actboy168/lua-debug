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
        std::atomic_bool inwatch = false;
        bool hook() override {
            for (auto &&watch: wather_points) {
                if (watch.address) {
                    int ec = DobbyInstrument(watch.address, default_watch);
					LOG(std::format("DobbyInstrument:{}[{}] {}",watch.address, watch.funcname, ec).c_str());
				}
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
                get_watch_symbol(watch, resolver);
            }
            for (auto &&watch: wather_points) {
                if (watch.address)
                    return true;
            }
            return false;
        }

        static inline void get_watch_symbol(watch_point& watch,const std::unique_ptr<symbol_resolver::interface> &resolver){
            watch.address = resolver->getsymbol(watch.funcname.c_str());
        }

        static void attach_lua_Hooker(state_t *L, debug_t *ar) {
            attach_lua_vm((state_t*)L);
            auto &_self = get_this();
            _self.hooker.call_origin_hook(L, ar);
            //inject success disable hook
            _self.unhook();
        }

        void watch_entry(state_t *L) {
            bool test = false;
            if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
                hooker.call_lua_sethook(L, attach_lua_Hooker);
            }
        }

        static void default_watch(void *address, DobbyRegisterContext *ctx) {
            auto L = (state_t *) getfirstarg(ctx);
            auto &_self = get_this();
            _self.watch_entry(L);           
        }

        static vmhook_template &get_this();
    };
}