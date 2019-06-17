#include <lua.hpp>
#include <Windows.h>
#include <base/hook/injectdll.h>
#include <base/hook/replacedll.h>
#include <bee/lua/binding.h>
#include <bee/utility/unicode_win.h>
#include <bee/subprocess.h>
#include <algorithm>
#include "query_process.h"

static int injectdll(lua_State* L) {
    const char* entry = 0;
    if (lua_gettop(L) >= 4) {
        entry = luaL_checkstring(L, 4);
    }
    if (lua_type(L, 1) == LUA_TNUMBER) {
        DWORD pid = (DWORD)luaL_checkinteger(L, 1);
        bool ok = base::hook::injectdll(pid, bee::lua::to_string(L, 2), bee::lua::to_string(L, 3), entry);
        lua_pushboolean(L, ok);
        return 1;
    }
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = base::hook::injectdll(self.info(), bee::lua::to_string(L, 2), bee::lua::to_string(L, 3), entry);
    lua_pushboolean(L, ok);
    return 1;
}

static int replacedll(lua_State* L) {
    auto& self = *(bee::subprocess::process*)getObject(L, 1, "subprocess");
    bool ok = base::hook::replacedll(self.info(), luaL_checkstring(L, 2), bee::u2a(luaL_checkstring(L, 3)).c_str());
    lua_pushboolean(L, ok);
    return 1;
}

static int query_process(lua_State* L) {
	std::wstring name = bee::lua::to_string(L, 1);
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    lua_newtable(L);
	query_process([&](const SYSTEM_PROCESS_INFORMATION* info)->bool {
		std::wstring pname(info->ImageName.Buffer, info->ImageName.Length / sizeof(wchar_t));
		std::transform(pname.begin(), pname.end(), pname.begin(), ::tolower);
		if (pname == name) {
            lua_pushinteger(L, (lua_Integer)(intptr_t)info->ProcessId);
            lua_seti(L, -2, luaL_len(L, -2) + 1);
		}
		return false;
	});
    return 1;
}

extern "C" __declspec(dllexport)
int luaopen_inject(lua_State* L) {
    luaL_Reg lib[] = {
        {"injectdll", injectdll},
        {"replacedll", replacedll},
        {"query_process", query_process},
        {NULL, NULL},
    };
    luaL_newlib(L, lib);
    return 1;
}
