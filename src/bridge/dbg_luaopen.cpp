#include "bridge/dbg_delayload.h"
#include "dbg_lua.h"
#include "debugger.h"
#include "dbg_network.h"
#include <memory>
#include <intrin.h>  


namespace luaw {
	struct ud {
		std::unique_ptr<vscode::network>  io;
		std::unique_ptr<vscode::debugger> dbg;

		void listen(const char* ip, uint16_t port, bool rebind)
		{
			if (io || dbg) return;
			io.reset(new vscode::network(ip, port, rebind));
			dbg.reset(new vscode::debugger(io.get(), vscode::threadmode::async));
		}
	};

	static ud& to(lua_State* L, int idx)
	{
		return *(ud*)luaL_checkudata(L, idx, "vscode::debugger");
	}

	static int constructor(lua_State* L)
	{
		void* storage = lua_newuserdata(L, sizeof(ud));
		luaL_setmetatable(L, "vscode::debugger");
		new (storage)ud();
		return 1;
	}

	static int listen(lua_State* L)
	{
		ud& self = to(L, 1);
		self.listen(luaL_checkstring(L, 2), (uint16_t)luaL_checkinteger(L, 3), lua_toboolean(L, 4));
		if (self.dbg) {
			self.dbg->attach_lua(L);
		}
		return 0;
	}

	static int start(lua_State* L)
	{
		ud& self = to(L, 1);
		if (!self.dbg) {
			return 0;
		}
		self.dbg->wait_attach();
		return 0;
	}

	static int port(lua_State* L)
	{
		ud& self = to(L, 1);
		if (!self.io) {
			return 0;
		}
		lua_pushinteger(L, self.io->get_port());
		return 1;
	}

	static int config(lua_State* L)
	{
		ud& self = to(L, 1);
		if (!self.dbg) {
			return 0;
		}
		if (lua_type(L, 2) == LUA_TSTRING) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 2, &len);
			std::string err = "unknown";
			if (!self.dbg->set_config(0, std::string(str, len), err)) {
				lua_pushlstring(L, err.data(), err.size());
				return lua_error(L);
			}
		}
		if (lua_type(L, 3) == LUA_TSTRING) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 3, &len);
			std::string err = "unknown";
			if (!self.dbg->set_config(2, std::string(str, len), err)) {
				lua_pushlstring(L, err.data(), err.size());
				return lua_error(L);
			}
		}
		return 0;
	}

	static int mt_gc(lua_State* L)
	{
		ud& self = to(L, 1);
		if (self.dbg) {
			self.dbg->detach_lua(L);
		}
		self.~ud();
		return 0;
	}

	int open(lua_State* L)
	{
		luaL_checkversion(L);
		luaL_Reg mt[] = {
			{ "listen", listen },
			{ "start", start },
			{ "port", port },
			{ "config", config },
			{ "__gc", mt_gc },
			{ NULL, NULL },
		};
		luaL_newmetatable(L, "vscode::debugger");
		luaL_setfuncs(L, mt, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		return constructor(L);
	}
}

#if defined(DEBUGGER_BRIDGE)
static void caller_is_luadll(void* callerAddress)
{
	HMODULE  caller = NULL;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)callerAddress, &caller) && caller)
	{
		if (GetProcAddress(caller, "lua_newstate"))
		{
			delayload::set_luadll(caller, ::GetProcAddress);
		}
	}
}
#endif

int __cdecl luaopen_debugger(lua_State* L)
{
#if defined(DEBUGGER_BRIDGE)
	caller_is_luadll(_ReturnAddress());
#endif
	return luaw::open(L);
}
