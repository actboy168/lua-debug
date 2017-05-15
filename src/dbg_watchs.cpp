#include "dbg_watchs.h"
#include <lua.hpp>

namespace vscode
{
	intptr_t WATCH_TABLE = 0;

	watchs::watchs()
		: cur_(0)
		, max_(0)
	{
	}

	watchs::~watchs()
	{
		//clear();
	}

	size_t watchs::add(lua_State* L)
	{
		if (max_ < 250)
		{
			max_++;
		}
		else if (cur_ == max_)
		{
			cur_ = 0;
		}

		t_set(L, ++cur_);
		return cur_;
	}

	bool watchs::get(lua_State* L, size_t index)
	{
		if (index > max_ || index == 0)
		{
			return false;
		}
		t_get(L, index);
		return true;
	}

	void watchs::clear(lua_State* L)
	{
		lua_pushnil(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE);
		cur_ = 0;
		max_ = 0;
	}

	void watchs::t_table(lua_State* L)
	{
		if (LUA_TTABLE != lua_rawgetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE)) {
			lua_pop(L, 1);
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_rawsetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE);
		}
	}

	void watchs::t_set(lua_State* L, int n)
	{
		int top1 = lua_gettop(L);
		t_table(L);
		lua_pushvalue(L, -2);
		lua_rawseti(L, -2, n);
		lua_pop(L, 1);
		int top2 = lua_gettop(L);
	}

	void watchs::t_get(lua_State* L, int n)
	{
		int top1 = lua_gettop(L);
		t_table(L);
		lua_rawgeti(L, -1, n);
		lua_remove(L, -2);
		int t = lua_type(L, -1);
		int top2 = lua_gettop(L);
	}
}
