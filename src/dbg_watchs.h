#pragma once

struct lua_State;

namespace vscode
{
	class watchs
	{
	public:
		watchs(lua_State* L);
		~watchs();
		size_t add();
		bool get(size_t index);
		void clear();
		void destory();

	private:
		void t_table();
		void t_set(int n);
		void t_get(int n);

	private:
		lua_State* L;
		size_t cur_;
		size_t max_;
	};
}
