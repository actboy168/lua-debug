#include <Windows.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/subprocess.h>
#include <bee/win/wtf8.h>

#include <algorithm>
#include <lua.hpp>
#include <string_view>

#include "injectdll.h"

static std::string_view checkstrview(lua_State* L, int idx) {
    size_t len      = 0;
    const char* buf = luaL_checklstring(L, idx, &len);
    return { buf, len };
}

static std::wstring checkstring(lua_State* L, int idx) {
    auto str = checkstrview(L, idx);
    return bee::wtf8::u2w(str);
}

static int injectdll(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        DWORD pid = (DWORD)luaL_checkinteger(L, 1);
        bool ok   = injectdll(pid, checkstring(L, 2), checkstring(L, 3), checkstrview(L, 4));
        lua_pushboolean(L, ok);
        return 1;
    } else {
        auto& self = bee::lua::checkudata<bee::subprocess::process>(L, 1);
        bool ok    = injectdll(self.native_handle(), self.thread_handle(), checkstring(L, 2), checkstring(L, 3), checkstrview(L, 4));
        lua_pushboolean(L, ok);
        return 1;
    }
}

extern "C" int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        { "injectdll", injectdll },
        { NULL, NULL },
    };
    luaL_newlibtable(L, lib);
    luaL_setfuncs(L, lib, 0);
    return 1;
}

static ::bee::lua::callfunc _init(::bee::lua::register_module, "inject", luaopen_inject);
