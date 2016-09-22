#pragma once

#include <lua.hpp>

namespace vscode
{
	inline bool evaluate(lua_State* L, lua_Debug *ar, const char* script)
	{
		if (luaL_loadstring(L, script))
		{
			return false;
		}

		lua_newtable(L);
		{
			lua_newtable(L);
			lua_pushglobaltable(L);
			lua_setfield(L, -2, "__index");
			lua_setmetatable(L, -2);
		}

		if (lua_getinfo(L, "f", ar))
		{
			for (int i = 1;; ++i)
			{
				const char* name = lua_getupvalue(L, -1, i);
				if (!name) break;
				lua_setfield(L, -3, name);
			}
			lua_pop(L, 1);
		}

		for (int i = 1;; ++i)
		{
			const char* name = lua_getlocal(L, ar, -i);
			if (!name) break;
			lua_setfield(L, -2, name);
		}

		for (int i = 1;; ++i)
		{
			const char* name = lua_getlocal(L, ar, i);
			if (!name)
			{
				break;
			}
			lua_setfield(L, -2, name);
		}

		if (!lua_setupvalue(L, -2, 1))
		{
			lua_pop(L, 1);
		}

		if (lua_pcall(L, 0, LUA_MULTRET, 0))
		{
			return false;
		}
		return true;
	}
}
