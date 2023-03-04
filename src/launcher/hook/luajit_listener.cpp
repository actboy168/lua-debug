#include <lj_jit.h>
#include <lj_obj.h>

#include <hook/luajit_listener.hpp>

namespace luadebug::autoattach {
static lua::state jit2state(void* ctx) {
    auto j = (jit_State*)ctx;
    return (lua::state)j->L;
}
static lua::state global2state(void* ctx) {
    auto g = (global_State*)ctx;
    return (lua::state)gco2th(gcref(g->cur_L));
}

void luajit_global_listener::on_enter(Gum::InvocationContext* context) {
    hooker->watch_entry(global2state(context->get_nth_argument_ptr(0)));
}

void luajit_jit_listener::on_enter(Gum::InvocationContext* context) {
    hooker->watch_entry(jit2state(context->get_nth_argument_ptr(0)));
}

}
