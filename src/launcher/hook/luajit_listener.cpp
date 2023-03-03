#include <lj_jit.h>
#include <lj_obj.h>

#include <hook/luajit_listener.hpp>

namespace luadebug::autoattach {
static void* jit2state(void* ctx) {
  auto j = (jit_State*)ctx;
  return (void*)j->L;
}
static void* global2state(void* ctx) {
  auto g = (global_State*)ctx;
  return (void*)gco2th(gcref(g->cur_L));
}

void luajit_global_listener::on_enter(Gum::InvocationContext* context) {
  global2state(context->get_nth_argument_ptr(0));
}

void luajit_jit_listener::on_enter(Gum::InvocationContext* context) {
  jit2state(context->get_nth_argument_ptr(0));
}

}  // namespace autoattach