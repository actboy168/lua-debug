#include <lua.hpp>
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
#ifdef _WIN32
        DWORD pid = (DWORD)luaL_checkinteger(L, 1);
        bool ok = injectdll(pid, bee::lua::to_string(L, 2), bee::lua::to_string(L, 3), entry);
#else
        pid_t pid = (pid_t)luaL_checkinteger(L, 1);
        bool ok = injectdll(pid, bee::lua::to_string(L, 2), entry);
#endif

        lua_pushboolean(L, ok);
        return 1;
    }
#ifdef _WIN32
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = injectdll(self.info(), bee::lua::to_string(L, 2), bee::lua::to_string(L, 3), entry);
    lua_pushboolean(L, ok);
    return 1;
#endif
    return 0;
}
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
extern "C" DLLEXPORT
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
