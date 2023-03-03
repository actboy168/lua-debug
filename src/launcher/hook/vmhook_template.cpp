#include <hook/vmhook_template.hpp>

#include <hook/unknown.hpp>
#include <hook/hook_common.h>
#include <resolver/lua_delayload.h>
#include <util/log.h>
#include <bee/nonstd/format.h>

namespace luadebug::autoattach {
    void attach_lua_Hooker(lua::state L, lua::debug ar);

    void get_watch_symbol(watch_point& watch, const lua::resolver& resolver) {
        watch.address = (void*)resolver.find(watch.funcname);
    }

    vmhook_template::vmhook_template(Gum::RefPtr<Gum::Interceptor> in) 
        : interceptor{in}
    {}

    bool vmhook_template::hook() {
        for (auto &&watch: wather_points) {
            if (watch.address) {
                if (!interceptor->attach(watch.address, this, (void*)this))
                    log::info("interceptor attach failed:{}[{}]", watch.address, watch.funcname);
            }
        }
        return true;
    }

    void vmhook_template::unhook() {
        interceptor->detach(this);
    }

    bool vmhook_template::get_symbols(const lua::resolver& resolver) {
        for (auto &&watch: wather_points) {
            get_watch_symbol(watch, resolver);
        }
        for (auto &&watch: wather_points) {
            if (watch.address)
                return true;
        }
        return false;
    }

    void vmhook_template::watch_entry(lua::state L) {
        bool test = false;
        if (inwatch.compare_exchange_strong(test, true, std::memory_order_acquire)) {
            hooker.call_lua_sethook(L, attach_lua_Hooker);
        }
    }

    void vmhook_template::on_enter (Gum::InvocationContext * context) {
        auto L = context->get_nth_argument<lua::state>(0);
        watch_entry(L);
    }
}
