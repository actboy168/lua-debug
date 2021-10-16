#define RLUA_REPLACE
#include "rdebug_cmodule.h"
#include <bee/lua/binding.h>

extern "C" int luaopen_remotedebug_hookmgr(rlua_State* L);
extern "C" int luaopen_remotedebug_socket(rlua_State* L);
extern "C" int luaopen_remotedebug_stdio(rlua_State* L);
extern "C" int luaopen_remotedebug_thread(rlua_State* L);
extern "C" int luaopen_remotedebug_utility(rlua_State* L);
extern "C" int luaopen_remotedebug_visitor(rlua_State* L);

static rluaL_Reg cmodule[] = {
    {"remotedebug.hookmgr", luaopen_remotedebug_hookmgr},
    {"remotedebug.socket", luaopen_remotedebug_socket},
    {"remotedebug.stdio", luaopen_remotedebug_stdio},
    {"remotedebug.thread", luaopen_remotedebug_thread},
    {"remotedebug.utility", luaopen_remotedebug_utility},
    {"remotedebug.visitor", luaopen_remotedebug_visitor},
    {NULL, NULL},
};

namespace remotedebug {
    static void require_cmodule() {
        for (const rluaL_Reg* l = cmodule; l->name != NULL; l++) {
            ::bee::lua::register_module(l->name, l->func);
        }
    }
    static ::bee::lua::callfunc _init(require_cmodule);
    int require_all(rlua_State* L) {
        ::bee::lua::preload_module(L);
        return 0;
    }
}
