#pragma once

#include <debugger/lua.h>

namespace vscode {
	struct source;

	struct vdebugMgr {
		source* s = nullptr;
		void event_call(source* source);
		void event_return();
		source* get_source();
	};

	struct debug {
		lua_State* lua;
		lua::Debug* ar;
		lua::Debug* vr;
		int         scope = -1;

		debug(lua_State* l, lua::Debug* a);
		debug(lua_State* l);
		debug(debug& o);
		~debug();

		static debug event_call(lua_State* l);
		static debug event_return(lua_State* l);
		static debug event_line(lua_State* l, int currentline, int scope);

		bool is_virtual();
		lua_State* L();
		lua::Debug* value();
		int event();
		int currentline();
		int get_scope();
	};
}
