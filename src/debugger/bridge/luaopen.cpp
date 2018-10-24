#include <debugger/bridge/delayload.h>
#include <debugger/lua.h>
#include <debugger/debugger.h>
#include <debugger/io/socket.h>
#include <debugger/io/namedpipe.h>
#include <base/util/unicode.h>
#include <memory>  

#if defined(DEBUGGER_BRIDGE)
#include <intrin.h>
#endif

static int DBG = 0;

namespace luaw {
	struct ud {
		std::unique_ptr<vscode::io::socket_s> socket_s;
		std::unique_ptr<vscode::io::socket_c> socket_c;
		std::unique_ptr<vscode::io::namedpipe> namedpipe;
		std::unique_ptr<vscode::debugger> dbg;
		bool guard = false;

		void listen_tcp(const char* addr)
		{
			if (namedpipe || dbg) return;
			socket_s.reset(new vscode::io::socket_s(addr));
			dbg.reset(new vscode::debugger(socket_s.get()));
		}

		void connect_tcp(const char* addr)
		{
			if (namedpipe || dbg) return;
			socket_c.reset(new vscode::io::socket_c(addr));
			dbg.reset(new vscode::debugger(socket_c.get()));
		}

#if defined(_WIN32)
		void listen_pipe(const wchar_t* name)
		{
			if (namedpipe || dbg) return;
			namedpipe.reset(new vscode::io::namedpipe());
			namedpipe->open_server(name);
			dbg.reset(new vscode::debugger(namedpipe.get()));
		}
#endif
	};

	static std::unique_ptr<ud> global;

	static ud& get()
	{
		if (!global) {
			global.reset(new ud);
		}
		return *global;
	}

	static int constructor(lua_State* L)
	{
		lua_newuserdata(L, 1);
		luaL_setmetatable(L, "vscode::debugger");
		return 1;
	}

	static int io(lua_State* L)
	{
		ud& self = get();
		const char* addr = luaL_checkstring(L, 2);
		if (strncmp(addr, "listen:", 7) == 0) {
			self.listen_tcp(addr + 7);
		}
		else if (strncmp(addr, "connect:", 8) == 0) {
			self.connect_tcp(addr + 8);
		}
#if defined(_WIN32)
		else if (strncmp(addr, "pipe:", 5) == 0) {
			self.listen_pipe(base::u2w(addr + 5).c_str());
		}
#endif
		else {
			self.listen_tcp(addr);
		}
		lua_pushvalue(L, 1);
		return 1;
	}

	static int wait(lua_State* L)
	{
		ud& self = get();
		if (!self.dbg) {
			lua_pushvalue(L, 1);
			return 1;
		}
		self.dbg->wait_client();
		lua_pushvalue(L, 1);
		return 1;
	}

	static int start(lua_State* L)
	{
		ud& self = get();
		if (!self.dbg) {
			lua_pushvalue(L, 1);
			return 1;
		}
		self.dbg->attach_lua(L);
		lua_pushvalue(L, 1);
		return 1;
	}

	static int config(lua_State* L)
	{
		ud& self = get();
		if (!self.dbg) {
			lua_pushvalue(L, 1);
			return 1;
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
		lua_pushvalue(L, 1);
		return 1;
	}

	static int redirect(lua_State* L)
	{
		ud& self = get();
		if (!self.dbg) {
			lua_pushvalue(L, 1);
			return 1;
		}
		if (lua_type(L, 2) == LUA_TSTRING) {
			const char* type = luaL_checkstring(L, 2);
			if (strcmp(type, "stdout") == 0) {
				self.dbg->open_redirect(vscode::eRedirect::stdoutput);
			}
			else if (strcmp(type, "stderr") == 0) {
				self.dbg->open_redirect(vscode::eRedirect::stderror);
			}
			else if (strcmp(type, "print") == 0) {
				self.dbg->open_redirect(vscode::eRedirect::print, L);
			}
		}
		lua_pushvalue(L, 1);
		return 1;
	}

	static int guard(lua_State* L)
	{
		ud& self = get();
		self.guard = true;
		lua_pushvalue(L, 1);
		return 1;
	}

	static int event(lua_State* L)
	{
		ud& self = get();
		self.dbg->event(luaL_checkstring(L, 2), L, 3, lua_gettop(L));
		return 0;
	}

	static int mt_gc(lua_State* L)
	{
		ud& self = get();
		if (self.dbg) {
			self.dbg->detach_lua(L);
		}
		if (self.guard) {
			self.dbg.reset();
			self.socket_s.reset();
			self.socket_c.reset();
			self.namedpipe.reset();
		}
		return 0;
	}

	int open(lua_State* L)
	{
		luaL_Reg mt[] = {
			{ "io", io },
			{ "wait", wait },
			{ "start", start },
			{ "config", config },
			{ "redirect", redirect },
			{ "guard", guard },
			{ "event", event },
			{ "__gc", mt_gc },
			{ NULL, NULL },
		};
		luaL_newmetatable(L, "vscode::debugger");
		luaL_setfuncs(L, mt, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pop(L, 1);
		return constructor(L);
	}
}

#if defined(_WIN32)
void debugger_create(const wchar_t* name)
{
	luaw::get().listen_pipe(name);
}
vscode::debugger* debugger_get()
{
	return luaw::get().dbg.get();
}
#endif

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

int luaopen_debugger(lua_State* L)
{
#if defined(DEBUGGER_BRIDGE)
	caller_is_luadll(_ReturnAddress());
#endif
	if (lua_rawgetp(L, LUA_REGISTRYINDEX, &DBG) != LUA_TNIL) {
		return 1;
	}
	lua_pop(L, 1);

	luaw::open(L);
	lua_pushvalue(L, -1);
	lua_rawsetp(L, LUA_REGISTRYINDEX, &DBG);
	return 1;
}
