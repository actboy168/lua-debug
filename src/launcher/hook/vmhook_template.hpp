#pragma once

#include <hook/unknown.hpp>
#include <resolver/lua_delayload.h>
#include <util/log.h>

#include <vector>
#include <string>
#include <atomic>

namespace luadebug::autoattach {
    struct watch_point {
        std::string funcname;
        void *address;
    };
    static void attach_lua_Hooker(lua::state L, lua::debug ar);
    struct vmhook_template : vmhook ,Gum::NoLeaveInvocationListener{
		vmhook_template(Gum::RefPtr<Gum::Interceptor> in) : interceptor{in} {}
		vmhook_template(const vmhook_template&) = delete;
        std::vector <watch_point> wather_points;
		vmhooker hooker;
        std::atomic_bool inwatch = false;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        bool hook() override {
            for (auto &&watch: wather_points) {
                if (watch.address) {
                    if (!interceptor->attach(watch.address, this, (void*)this))
					    log::info(std::format("interceptor attach failed:{}[{}]",watch.address, watch.funcname).c_str());
				}
            }
			return true;
        }

        void unhook() override {
           interceptor->detach(this);
        }

        bool get_symbols(const lua::resolver& resolver) override {
            for (auto &&watch: wather_points) {
                get_watch_symbol(watch, resolver);
            }
            for (auto &&watch: wather_points) {
                if (watch.address)
                    return true;
            }
            return false;
        }

        static inline void get_watch_symbol(watch_point& watch, const lua::resolver& resolver){
            watch.address = (void*)resolver.find(watch.funcname.c_str());
        }

        void watch_entry(lua::state L) {
            bool test = false;
            if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
                hooker.call_lua_sethook(L, attach_lua_Hooker);
            }
        }

        void on_enter (Gum::InvocationContext * context) override {
            auto L = context->get_nth_argument<lua::state>(0);
            watch_entry(L);
        }

    };
}