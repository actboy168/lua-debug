#include <lua.hpp>
#include <bee/lua/binding.h>
#include <sys/types.h>
#include <unistd.h>

static int injectdll(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

static int replacedll(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

static int query_process(lua_State* L) {
    lua_newtable(L);
    return 1;
}

static int current_pid(lua_State* L) {
    lua_pushinteger(L, getpid());
    return 1;
}

extern "C"
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {"replacedll", replacedll},
        {"query_process", query_process},
        {"current_pid", current_pid},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
