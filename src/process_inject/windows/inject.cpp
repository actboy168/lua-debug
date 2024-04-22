#include <Windows.h>
#include <bee/lua/binding.h>
#include <bee/win/wtf8.h>
#include <bee/subprocess.h>
#include <bee/utility/zstring_view.h>
#include <binding/binding.h>

#include <algorithm>
#include <lua.hpp>

#include "injectdll.h"

static bee::zstring_view checkstrview(lua_State* L, int idx) {
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
    }
    else {
        auto& self = *(bee::subprocess::process*)luaL_checkudata(L, 1, "bee::subprocess");
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

#include <binding/binding.h>
static ::bee::lua::callfunc _init(::bee::lua::register_module, "inject", luaopen_inject);
