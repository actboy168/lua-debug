#include <hook/listener.h>
#include <hook/luajit_listener.h>
#include <hook/watchdog.h>

namespace luadebug::autoattach {

    void common_listener::on_enter(Gum::InvocationContext* context) {
        watchdog* w = (watchdog*)context->get_listener_function_data_ptr();
        w->watch_entry((uintptr_t)context->get_nth_argument_ptr(0));
    }

    void luajit_global_listener::on_enter(Gum::InvocationContext* context) {
        watchdog* w = (watchdog*)context->get_listener_function_data_ptr();
        w->watch_entry(global2state(context->get_nth_argument_ptr(0)));
    }

    void luajit_jit_listener::on_enter(Gum::InvocationContext* context) {
        watchdog* w = (watchdog*)context->get_listener_function_data_ptr();
        w->watch_entry(jit2state(context->get_nth_argument_ptr(0)));
    }

}
