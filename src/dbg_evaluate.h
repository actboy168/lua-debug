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

	bool evaluate(lua_State* L, lua_Debug *ar, const char* script, int& nresult, bool writeable = false);
}
