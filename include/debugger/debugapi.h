#pragma once

#include <debugger/lua.h>

namespace vscode {
	struct vdebugMgr {
		source* s = nullptr;
		void event_call(source* source) {
			s = source;
		}
		void event_return() {
			s = nullptr;
		}

		source* get_source() {
			return s;
		}
	};

	struct debug {
		lua_State* lua;
		lua::Debug* ar;
		lua::Debug* vr;

		debug(lua_State* l, lua::Debug* a)
			: lua(l), ar(a), vr(0)
		{ }

		debug(lua_State* l)
			: lua(l), ar(0), vr(new lua::Debug)
		{ }

		debug(debug& o)
			: lua(o.lua), ar(o.ar), vr(o.vr)
		{
			o.lua = 0;
			o.ar = 0;
			o.vr = 0;
		}

		~debug() {
			delete vr;
		}

		static debug event_call(lua_State* l) {
			debug d(l);
			d.vr->event = LUA_HOOKCALL;
			return d;
		}
		static debug event_return(lua_State* l) {
			debug d(l);
			d.vr->event = LUA_HOOKRET;
			return d;
		}
		static debug event_line(lua_State* l, int currentline) {
			debug d(l);
			d.vr->event = LUA_HOOKLINE;
			d.vr->currentline = currentline;
			return d;
		}

		bool is_virtual() {
			return ar ? false: true;
		}
		lua_State* L() {
			return lua;
		}
		lua::Debug* value() {
			return ar;
		}
		int event() {
			return is_virtual() ? vr->event : ar->event;
		}
		int currentline() {
			return is_virtual() ? vr->currentline : ar->currentline;
		}
	};
}
