#include "debugger.h"
#include "dbg_impl.h"
#include "dbg_network.h"
#include "bridge/dbg_delayload.h"
#include <base/util/unicode.h>

namespace vscode
{
	debugger::debugger(io* io, threadmode mode)
		: impl_(new debugger_impl(io, mode))
	{ }

	debugger::~debugger()
	{
		delete impl_;
	}

	void debugger::close()
	{
		impl_->close();
	}

	void debugger::update()
	{
		impl_->update();
	}

	void debugger::wait_attach()
	{
		impl_->wait_attach();
	}

	void debugger::attach_lua(lua_State* L)
	{
		impl_->attach_lua(L);
	}

	void debugger::detach_lua(lua_State* L)
	{
		impl_->detach_lua(L);
	}

	void debugger::set_custom(custom* custom)
	{
		impl_->set_custom(custom);
	}

	void debugger::output(const char* category, const char* buf, size_t len, lua_State* L)
	{
		impl_->output(category, buf, len, L);
	}

	void debugger::exception(lua_State* L)
	{
		impl_->exception(L);
	}

	bool debugger::is_state(state state) const
	{
		return impl_->is_state(state);
	}

	void debugger::redirect_stdout()
	{
		impl_->redirect_stdout();
	}

	void debugger::redirect_stderr()
	{
		impl_->redirect_stderr();
	}

	bool debugger::set_config(int level, const std::string& cfg, std::string& err) 
	{
		return impl_->set_config(level, cfg, err);
	}
}

std::unique_ptr<vscode::network>  global_io;
std::unique_ptr<vscode::debugger> global_dbg;

void __cdecl debugger_set_luadll(void* luadll, void* getluaapi)
{
#if defined(DEBUGGER_DELAYLOAD_LUA)
	delayload::set_luadll((HMODULE)luadll, (GetLuaApi)getluaapi);
#endif
}

void __cdecl debugger_start_server(const char* ip, uint16_t port, bool launch, bool rebind)
{
	if (!global_io || !global_dbg)
	{
		global_io.reset(new vscode::network(ip, port, rebind));
		global_dbg.reset(new vscode::debugger(global_io.get(), vscode::threadmode::async));
		if (launch)
			global_io->kill_process_when_close();
	}
}

void __cdecl debugger_wait_attach()
{
	if (global_dbg) {
		global_dbg->wait_attach();
		global_dbg->redirect_stdout();
		global_dbg->redirect_stderr();
	}
}

void __cdecl debugger_attach_lua(lua_State* L)
{
	if (global_dbg) global_dbg->attach_lua(L);
}

void __cdecl debugger_detach_lua(lua_State* L)
{
	if (global_dbg) global_dbg->detach_lua(L);
}

namespace luaw {
	int listen(lua_State* L)
	{
		debugger_start_server(luaL_checkstring(L, 1), (uint16_t)luaL_checkinteger(L, 2), false, lua_toboolean(L, 3));
		debugger_attach_lua(L);
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int start(lua_State* L)
	{
		debugger_attach_lua(L);
		debugger_wait_attach();
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int port(lua_State* L)
	{
		if (global_io && global_dbg) {
			lua_pushinteger(L, global_io->get_port());
			return 1;
		}
		return 0;
	}

	int config(lua_State* L)
	{
		if (!global_io || !global_dbg) {
			return 0;
		}
		if (lua_type(L, 1) == LUA_TSTRING) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 1, &len);
			std::string err = "unknown";
			if (!global_dbg->set_config(0, std::string(str, len), err)) {
				lua_pushlstring(L, err.data(), err.size());
				return lua_error(L);
			}
		}
		if (lua_type(L, 2) == LUA_TSTRING) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 2, &len);
			std::string err = "unknown";
			if (!global_dbg->set_config(2, std::string(str, len), err)) {
				lua_pushlstring(L, err.data(), err.size());
				return lua_error(L);
			}
		}
		return 0;
	}

	int mt_gc(lua_State* L)
	{
		if (global_dbg) {
			global_dbg->detach_lua(L);
		}
		return 0;
	}

	int open(lua_State* L)
	{
		luaL_checkversion(L);
		luaL_Reg f[] = {
			{ "listen", listen },
			{ "start", start },
			{ "port", port },
			{ "config", config },
			{ "__gc", mt_gc },
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

#if defined(DEBUGGER_DELAYLOAD_LUA)
void caller_is_luadll(void* callerAddress)
{
	HMODULE  caller = NULL;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)callerAddress, &caller) && caller)
	{
		if (GetProcAddress(caller, "lua_newstate"))
		{
			delayload::set_luadll(caller);
		}
	}
}
#endif

int __cdecl luaopen_debugger(lua_State* L)
{
#if defined(DEBUGGER_DELAYLOAD_LUA)
	caller_is_luadll(_ReturnAddress());
#endif
	return luaw::open(L);
}
