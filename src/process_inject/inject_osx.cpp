#include <lua.hpp>
#include <bee/lua/binding.h>

static int injectdll(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

static int query_process(lua_State* L) {
    lua_newtable(L);
    return 1;
}

extern "C"
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {"query_process", query_process},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
