#include <Windows.h>
#include <bee/platform/win/unicode.h>
#include <bee/subprocess.h>
#include <binding/binding.h>

#include <algorithm>
#include <lua.hpp>

#include "injectdll.h"

inline std::string_view checkstrview(lua_State* L, int idx) {
    size_t len      = 0;
    const char* buf = luaL_checklstring(L, idx, &len);
    return { buf, len };
}

static int injectdll(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        DWORD pid = (DWORD)luaL_checkinteger(L, 1);
        bool ok   = injectdll(pid, bee::lua::checkstring(L, 2), bee::lua::checkstring(L, 3), checkstrview(L, 4));
        lua_pushboolean(L, ok);
        return 1;
    }
    else {
        auto& self = *(bee::subprocess::process*)luaL_checkudata(L, 1, "bee::subprocess");
        bool ok    = injectdll(self.native_handle(), self.thread_handle(), bee::lua::checkstring(L, 2), bee::lua::checkstring(L, 3), checkstrview(L, 4));
        lua_pushboolean(L, ok);
        return 1;
    }
}

extern "C" int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        { "injectdll", injectdll },
        { NULL, NULL },
    };
    luaL_newlib(L, lib);
    return 1;
}

#include <binding/binding.h>
static ::bee::lua::callfunc _init(::bee::lua::register_module, "inject", luaopen_inject);
