#include <debugger/debugapi.h>
#include <debugger/source.h>

namespace vscode {
	void vdebugMgr::event_call(source* source) {
		s = source;
	}
	void vdebugMgr::event_return() {
		s = nullptr;
	}

	source* vdebugMgr::get_source() {
		return s;
	}

	debug::debug(lua_State* l, lua::Debug* a)
		: lua(l), ar(a), vr(0)
	{ }

	debug::debug(lua_State* l)
		: lua(l), ar(0), vr(new lua::Debug)
	{ }

	debug::debug(debug& o)
		: lua(o.lua), ar(o.ar), vr(o.vr), scope(o.scope)
	{
		o.lua = 0;
		o.ar = 0;
		o.vr = 0;
		o.scope = -1;
	}

	debug::~debug() {
		delete vr;
	}

	debug debug::event_call(lua_State* l) {
		debug d(l);
		d.vr->event = LUA_HOOKCALL;
		return d;
	}
	debug debug::event_return(lua_State* l) {
		debug d(l);
		d.vr->event = LUA_HOOKRET;
		return d;
	}
	debug debug::event_line(lua_State* l, int currentline, int scope) {
		debug d(l);
		d.vr->event = LUA_HOOKLINE;
		d.vr->currentline = currentline;
		d.scope = scope;
		return d;
	}

	bool debug::is_virtual() {
		return ar ? false : true;
	}
	lua_State* debug::L() {
		return lua;
	}
	lua::Debug* debug::value() {
		return ar;
	}
	int debug::event() {
		return is_virtual() ? vr->event : ar->event;
	}
	int debug::currentline() {
		return is_virtual() ? vr->currentline : ar->currentline;
	}
	int debug::get_scope() {
		return scope;
	}
	bool debug::get_stack(int frameId, lua::Debug* ar) {
		if (frameId == 0xFFFF) {
			return is_virtual();
		}
		if (!lua_getstack(lua, frameId, (lua_Debug*)ar)) {
			return false;
		}
		return true;
	}

}
