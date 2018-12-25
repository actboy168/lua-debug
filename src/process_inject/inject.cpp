#include <lua.hpp>
#include <Windows.h>
#include <base/hook/injectdll.h>
#include <base/hook/replacedll.h>
#include <bee/lua/binding.h>
#include <bee/utility/unicode.h>
#include <bee/subprocess.h>

static int injectdll(lua_State* L) {
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = base::hook::injectdll(self.info(), bee::lua::to_string(L, 2), bee::lua::to_string(L, 3));
    lua_pushboolean(L, ok);
    return 1;
}

static int replacedll(lua_State* L) {
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = base::hook::replacedll(self.info(), bee::u2a(luaL_checkstring(L, 2)).c_str(), luaL_checkstring(L, 3));
    lua_pushboolean(L, ok);
    return 1;
}

static int current_pid(lua_State* L) {
    lua_pushinteger(L, ::GetCurrentProcessId());
    return 1;
}

extern "C" __declspec(dllexport)
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {"replacedll", replacedll},
        {"current_pid", current_pid},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
