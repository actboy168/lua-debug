#include <lua.hpp>
#include <Windows.h>

static int current_pid(lua_State* L) {
    lua_pushinteger(L, ::GetCurrentProcessId());
    return 1;
}

extern "C" __declspec(dllexport)
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"current_pid", current_pid},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
