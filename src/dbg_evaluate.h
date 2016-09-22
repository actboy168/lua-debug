#pragma once

#include <lua.hpp>
#include <array>

namespace vscode
{
	struct watchs
	{
		watchs(lua_State* L)
			: L(L)
			, cache_()
			, n_(0)
		{ }

		size_t add()
		{
			if (n_ >= cache_.max_size())
			{
				lua_pop(L, 1);
				return 0;
			}
			cache_[n_++] = luaL_ref(L, LUA_REGISTRYINDEX);
			return n_;
		}

		bool get(size_t index)
		{
			if (index > n_ || index == 0)
			{
				return false;
			}
			lua_rawgeti(L, LUA_REGISTRYINDEX, cache_[index-1]);
			return true;
		}

		void clear()
		{
			for (size_t i = 0; i < n_; ++i)
			{
				luaL_unref(L, LUA_REGISTRYINDEX, cache_[i]);
			}
			n_ = 0;
		}

		lua_State* L;
		std::array<int, 250> cache_;
		size_t n_;
	};

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
