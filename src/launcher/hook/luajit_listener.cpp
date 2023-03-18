#include <hook/luajit_listener.h>
#include <lj_jit.h>
#include <lj_obj.h>

namespace luadebug::autoattach {
    uintptr_t jit2state(void* ctx) {
        auto j = (jit_State*)ctx;
        return (uintptr_t)j->L;
    }
    uintptr_t global2state(void* ctx) {
        auto g = (global_State*)ctx;
        return (uintptr_t)gco2th(gcref(g->cur_L));
    }
}
