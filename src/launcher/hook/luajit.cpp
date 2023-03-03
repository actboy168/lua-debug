#include <memory>

#include <hook/luajit_listener.hpp>
#include <hook/vmhook_template.hpp>
#include <gumpp.hpp>

namespace luadebug::autoattach {
    struct luajit_hook : vmhook_template {
        luajit_hook(Gum::RefPtr<Gum::Interceptor> in) : vmhook_template{in} {}
        ~luajit_hook() = default;
        watch_point lj_dispatch_update{ "lj_dispatch_update"};
        watch_point lj_dispatch_stitch{ "lj_dispatch_stitch"};
        luajit_global_listener global_listener;
        luajit_jit_listener jit_listener;
        virtual bool get_symbols(const lua::resolver& resolver) override{
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
    
    std::unique_ptr<vmhook_template> create_luajit_vmhook(Gum::RefPtr<Gum::Interceptor> in) {
        return std::make_unique<luajit_hook>(in);
    }
}
