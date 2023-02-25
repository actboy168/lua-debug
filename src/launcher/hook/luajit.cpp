#include <memory>

#include "luajit_listener.hpp"
#include "vmhook_template.hpp"
#include <gumpp.hpp>
namespace autoattach{
    struct luajit_hook : vmhook_template {
        luajit_hook() = default;
        ~luajit_hook() = default;
        watch_point lj_dispatch_update{ "lj_dispatch_update"};
        watch_point lj_dispatch_stitch{ "lj_dispatch_stitch"};
        luajit_global_listener global_listener;
        luajit_jit_listener jit_listener;
        virtual bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver) override{
            if (!vmhook_template::get_symbols(resolver))
                return false;
            get_watch_symbol(lj_dispatch_update, resolver);
            get_watch_symbol(lj_dispatch_stitch, resolver);
            return true;
        }

        virtual bool hook() override{
            if (!vmhook_template::hook())
                return false;
            interceptor->attach(lj_dispatch_update.address, &global_listener);
            interceptor->attach(lj_dispatch_stitch.address, &jit_listener);
            return true;
        }
    };
    
    std::unique_ptr<vmhook_template> create_luajit_vmhook() {
        return std::make_unique<luajit_hook>();
    }
}