#include <memory>

#include <lj_obj.h>
#include <lj_jit.h>

#include "vmhook_template.hpp"

namespace autoattach{
    struct luajit_hook : vmhook_template {
        luajit_hook() = default;
        ~luajit_hook() = default;
        watch_point lj_dispatch_update{ "lj_dispatch_update"};
        watch_point lj_dispatch_stitch{ "lj_dispatch_stitch"};
        virtual bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver) override{
            if (!vmhook_template::get_symbols(resolver))
                return false;
            return true;
            get_watch_symbol(lj_dispatch_update, resolver);
            get_watch_symbol(lj_dispatch_stitch, resolver);
        }

        virtual bool hook() override{
            if (!vmhook_template::hook())
                return false;
            DobbyInstrument(lj_dispatch_update.address, watch_lj_dispatch_update);
            DobbyInstrument(lj_dispatch_stitch.address, watch_lj_dispatch_stitch);
            return true;
        }

        static void watch_lj_dispatch_update(void *address, DobbyRegisterContext *ctx) {
            auto& _self = get_this();
            auto L = global2state(getfirstarg(ctx));
            _self.watch_entry(L);
        }

        static void watch_lj_dispatch_stitch(void *address, DobbyRegisterContext *ctx) {
            auto& _self = get_this();
            auto L = jit2state(getfirstarg(ctx));
            _self.watch_entry(L);
        }

        static lua_State* jit2state(void* ctx) {
            auto j = (jit_State*)ctx;
            return j->L;
        }
        static lua_State* global2state(void* ctx) {
            auto g = (global_State*)ctx;
            return gco2th(gcref(g->cur_L));
        }
    };
    
    std::unique_ptr<vmhook_template> create_luajit_vmhook() {
        return std::make_unique<luajit_hook>();
    }
}