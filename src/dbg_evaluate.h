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
			, cur_(0)
			, max_(0)
		{ }

		size_t add()
		{
			if (max_ < cache_.max_size())
			{
				max_++;
			}
			else
			{
				if (cur_ == max_)
				{
					cur_ = 0;
				}
				luaL_unref(L, LUA_REGISTRYINDEX, cache_[cur_]);
			}

			cache_[cur_++] = luaL_ref(L, LUA_REGISTRYINDEX);
			return cur_;
		}

		bool get(size_t index)
		{
			if (index > max_ || index == 0)
			{
				return false;
			}
			lua_rawgeti(L, LUA_REGISTRYINDEX, cache_[index-1]);
			return true;
		}

		void clear()
		{
			for (size_t i = 0; i < max_; ++i)
			{
				luaL_unref(L, LUA_REGISTRYINDEX, cache_[i]);
			}
			cur_ = 0;
			max_ = 0;
		}

		lua_State* L;
		std::array<int, 250> cache_;
		size_t cur_;
		size_t max_;
	};

	bool evaluate(lua_State* L, lua_Debug *ar, const char* script, int& nresult, bool writeable = false);
}
