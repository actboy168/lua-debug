#include <lua.hpp>
#include <bee/lua/binding.h>

static int injectdll(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

extern "C" __attribute__((visibility("default")))
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
