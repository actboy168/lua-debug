#include <debugger/bridge/delayload.h>
#include <debugger/lua.h>
#include <debugger/debugger.h>
#include <debugger/io/socket.h>
#include <debugger/io/namedpipe.h>
#include <base/util/unicode.h>
#include <memory>
#include <intrin.h>  

namespace luaw {
	struct ud {
		std::unique_ptr<vscode::io::socket> socket;
		std::unique_ptr<vscode::io::namedpipe> namedpipe;
		std::unique_ptr<vscode::debugger> dbg;

		uint16_t listen_tcp(const char* addr)
		{
			if (socket) return socket->get_port();
			if (dbg) return 0;
			socket.reset(new vscode::io::socket(addr));
			dbg.reset(new vscode::debugger(socket.get(), vscode::threadmode::async));
			return socket->get_port();
		}

		void listen_pipe(const char* name)
		{
			if (namedpipe || dbg) return;
			namedpipe.reset(new vscode::io::namedpipe());
			namedpipe->open_server(base::u2w(name));
			dbg.reset(new vscode::debugger(namedpipe.get(), vscode::threadmode::async));
		}
	};

	static std::unique_ptr<ud> global;

	static ud& to(lua_State* L, int idx)
	{
		return *global;
	}

	static int constructor(lua_State* L)
	{
		if (!global) {
			global.reset(new ud);
		}
		lua_newuserdata(L, 1);
		luaL_setmetatable(L, "vscode::debugger");
		return 1;
	}

	static int listen(lua_State* L)
	{
		ud& self = to(L, 1);
		const char* addr = luaL_checkstring(L, 2);
		if (strncmp(addr, "pipe:", 5) == 0) {
			self.listen_pipe(addr + 5);
			if (self.dbg) {
				self.dbg->attach_lua(L);
			}
			return 0;
		}
		else if (strncmp(addr, "tcp:", 4) == 0) {
			uint16_t port = self.listen_tcp(addr + 4);
			if (self.dbg) {
				self.dbg->attach_lua(L);
			}
			return 1;
		}
		else {
			uint16_t port = self.listen_tcp(addr);
			if (self.dbg) {
				self.dbg->attach_lua(L);
			}
			return 1;
		}
		return 0;
	}

	static int start(lua_State* L)
	{
		ud& self = to(L, 1);
		if (!self.dbg) {
			return 0;
		}
		self.dbg->detach_lua(L);
		self.dbg->wait_attach();
		self.dbg->attach_lua(L);
		return 0;
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
		return 0;
	}

	int open(lua_State* L)
	{
		luaL_checkversion(L);
		luaL_Reg mt[] = {
			{ "listen", listen },
			{ "start", start },
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
