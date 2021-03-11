#include "rdebug_cmodule.h"

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
    const rluaL_Reg* get_cmodule() {
        return cmodule;
    }
    static int require_cmodule(rlua_State *L) {
        const char* name = (const char*)rlua_touserdata(L, 1);
        rlua_CFunction f = (rlua_CFunction)rlua_touserdata(L, 2);
        rluaL_requiref(L, name, f, 0);
        return 0;
    }
    static int require_function(rlua_State* L, const char* name, rlua_CFunction f) {
        rlua_pushcfunction(L, require_cmodule);
        rlua_pushlightuserdata(L, (void*)name);
        rlua_pushlightuserdata(L, (void*)f);
        if (rlua_pcall(L, 2, 0, 0) != LUA_OK) {
            rlua_pop(L, 1);
            return 1;
        }
        return 0;
    }
    static int require_all(rlua_State* L, const rluaL_Reg* l) {
        for (; l->name != NULL; l++) {
            require_function(L, l->name, l->func);
        }
        return 0;
    }
    int require_all(rlua_State* L) {
        return require_all(L, get_cmodule());
    }
}
