#pragma once

struct lua_State;

namespace vscode
{
	class watchs
	{
	public:
		watchs();
		~watchs();
		size_t add(lua_State* L);
		bool get(lua_State* L, size_t index);
		void clear(lua_State* L);

	private:
		void t_table(lua_State* L);
		void t_set(lua_State* L, size_t n);
		void t_get(lua_State* L, size_t n);

	private:
		size_t cur_;
		size_t max_;
	};
}
