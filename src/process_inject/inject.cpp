#include <lua.hpp>
#include <Windows.h>
#include <bee/lua/binding.h>
#include <bee/utility/unicode_win.h>
#include <bee/subprocess.h>
#include <algorithm>
#include "injectdll.h"

static int injectdll(lua_State* L) {
    const char* entry = 0;
    if (lua_gettop(L) >= 4) {
        entry = luaL_checkstring(L, 4);
    }
    if (lua_type(L, 1) == LUA_TNUMBER) {
        DWORD pid = (DWORD)luaL_checkinteger(L, 1);
        bool ok = injectdll(pid, bee::lua::checkstring(L, 2), bee::lua::checkstring(L, 3), entry);
        lua_pushboolean(L, ok);
        return 1;
    }
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = injectdll(self.info(), bee::lua::checkstring(L, 2), bee::lua::checkstring(L, 3), entry);
    lua_pushboolean(L, ok);
    return 1;
}

extern "C" __declspec(dllexport)
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
