#include <debugger/io/server.h>
#include <debugger/io/client.h>
#include <debugger/bridge/delayload.h>
#include <debugger/lua.h>
#include <debugger/debugger.h>
#include <bee/utility/unicode.h>
#include <bee/net/endpoint.h>
#include <memory>  
#include <string_view>

#if defined(DEBUGGER_BRIDGE)
#include <intrin.h>
#endif

static int DBG = 0;

static std::string_view luaL_checkstrview(lua_State* L, int idx) {
	size_t len = 0;
	const char* str = luaL_checklstring(L, idx, &len);
	return std::string_view(str, len);
}

namespace luaw {
	struct ud {
		std::unique_ptr<vscode::io::server> socket_s;
        std::unique_ptr<vscode::io::client> socket_c;
		std::unique_ptr<vscode::debugger> dbg;
		bool guard = false;

		std::pair<std::string, int> split_address(const std::string& addr) {
			size_t pos = addr.find(':');
			if (pos == addr.npos) {
				return { addr, 0 };
			}
			else {
				return { addr.substr(0, pos), atoi(addr.substr(pos + 1).c_str()) };
			}
		}
        bool listen_tcp(lua_State* L, const char* addr)
		{
			if (dbg) return false;
			auto [ip, port] = split_address(addr);
			auto info = bee::net::endpoint::from_hostname(ip, port);
			if (!info) {
                if (L) luaL_error(L, "%s", info.error().c_str());
                return false;
			}
			socket_s.reset(new vscode::io::server(info.value()));
			dbg.reset(new vscode::debugger(socket_s.get()));
            return true;
		}
        bool connect_pipe(lua_State* L, const char* path)
        {
            if (dbg) return false;
            auto info = bee::net::endpoint::from_unixpath(path);
            if (!info) {
                if (L) luaL_error(L, "%s", info.error().c_str());
                return false;
            }
            socket_c.reset(new vscode::io::client(info.value()));
            dbg.reset(new vscode::debugger(socket_c.get()));
            return true;
        }
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
        bee::net::socket::initialize();
		ud& self = get();
		const char* addr = luaL_checkstring(L, 2);
		if (strncmp(addr, "listen:", 7) == 0) {
			self.listen_tcp(L, addr + 7);
		}
		else if (strncmp(addr, "pipe:", 5) == 0) {
            self.connect_pipe(L, addr + 5);
		}
		else {
			self.listen_tcp(L, addr);
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
            else if (strcmp(type, "io.write") == 0) {
                self.dbg->open_redirect(vscode::eRedirect::iowrite, L);
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

	static int exception(lua_State* L)
	{
		ud& self = get();
		std::string_view type_str = luaL_checkstrview(L, 2);
		vscode::eException type;
		if (type_str == "pcall") {
			type = vscode::eException::pcall;
		}
		else if (type_str == "xpcall") {
			type = vscode::eException::xpcall;
		}
		else {
			return luaL_error(L, "Unknown exception type: %s.", type_str.data());
		}
		luaL_checktype(L, 3, LUA_TSTRING);
		lua_pushvalue(L, 3);
		self.dbg->exception(L, type, (int)luaL_checkinteger(L, 4));
		return 0;
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
			{ "exception", exception },
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
bool debugger_create(const char* path)
{
	return luaw::get().connect_pipe(0, path);
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
