#include <lj_jit.h>
#include <lj_obj.h>

#include <hook/luajit_listener.hpp>

namespace luadebug::autoattach {
static vm_watcher::value_t jit2state(void* ctx) {
    auto j = (jit_State*)ctx;
    return (vm_watcher::value_t)j->L;
}
static vm_watcher::value_t global2state(void* ctx) {
    auto g = (global_State*)ctx;
    return (vm_watcher::value_t)gco2th(gcref(g->cur_L));
}

void luajit_global_listener::on_enter(Gum::InvocationContext* context) {
    watcher->watch_entry(global2state(context->get_nth_argument_ptr(0)));
}

void luajit_jit_listener::on_enter(Gum::InvocationContext* context) {
    watcher->watch_entry(jit2state(context->get_nth_argument_ptr(0)));
}

}
