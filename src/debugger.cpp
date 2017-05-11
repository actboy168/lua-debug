#include "debugger.h"
#include "dbg_impl.h"
#include "dbg_network.h"

namespace vscode
{
	debugger::debugger(io* io, threadmode mode)
		: impl_(new debugger_impl(io, mode))
	{ }

	debugger::~debugger()
	{
		delete impl_;
	}

	void debugger::update()
	{
		impl_->update();
	}

	void debugger::attach_lua(lua_State* L, bool pause)
	{
		impl_->attach_lua(L, pause);
	}

	void debugger::detach_lua(lua_State* L)
	{
		impl_->detach_lua(L);
	}

	void debugger::set_custom(custom* custom)
	{
		impl_->set_custom(custom);
	}

	void debugger::output(const char* category, const char* buf, size_t len)
	{
		impl_->output(category, buf, len);
	}
}

namespace luaw {
	std::unique_ptr<vscode::network>  io;
	std::unique_ptr<vscode::debugger> dbg;

	int listen(lua_State* L)
	{
		if (!io || !dbg) {
			const char* ip = luaL_checkstring(L, 1);
			uint16_t port = (uint16_t)luaL_checkinteger(L, 2);
			io.reset(new vscode::network(ip, port));
			dbg.reset(new vscode::debugger(io.get(), vscode::threadmode::async));
		}
		dbg->attach_lua(L, false);
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int start(lua_State* L)
	{
		if (dbg) {
			dbg->attach_lua(L, true);
		}
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int __gc(lua_State* L)
	{
		if (dbg) {
			dbg->detach_lua(L);
		}
		return 0;
	}

	int open(lua_State* L)
	{
		luaL_checkversion(L);
		luaL_Reg f[] = {
			{ "listen", listen },
			{ "start", start },
			{ "__gc", __gc },
			{ NULL, NULL },
		};
		luaL_newlibtable(L, f);
		lua_pushvalue(L, -1);
		luaL_setfuncs(L, f, 1);
		lua_pushvalue(L, -1);
		lua_setmetatable(L, -2);
		return 1;
	}
}

int __cdecl luaopen_debugger(lua_State* L)
{
	return luaw::open(L);
}
