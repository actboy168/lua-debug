#include "debugger.h"
#include "dbg_impl.h"
#include "dbg_network.h"
#include "dbg_delayload.h"
#include "dbg_unicode.h"
#include <intrin.h>  

namespace vscode
{
	debugger::debugger(io* io, threadmode mode, coding coding)
		: impl_(new debugger_impl(io, mode, coding))
	{ }

	debugger::~debugger()
	{
		delete impl_;
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

	void debugger::set_coding(coding coding)
	{
		impl_->set_coding(coding);
	}

	void debugger::output(const char* category, const char* buf, size_t len)
	{
		impl_->output(category, buf, len, nullptr);
	}

	bool debugger::is_state(state state) const
	{
		return impl_->is_state(state);
	}
}

std::unique_ptr<vscode::network>  global_io;
std::unique_ptr<vscode::debugger> global_dbg;
vscode::coding                    global_coding = vscode::coding::ansi;

void __cdecl debugger_set_luadll(void* luadll)
{
#if defined(DEBUGGER_DELAYLOAD_LUA)
	delayload::set_luadll((HMODULE)luadll);
#endif
}

void __cdecl debugger_set_coding(int coding)
{
	global_coding = coding == 0 ? vscode::coding::ansi : vscode::coding::utf8;
	if (global_dbg) {
		global_dbg->set_coding(global_coding);
	}
}

void __cdecl debugger_start_server(const char* ip, uint16_t port, bool launch, bool rebind)
{
	if (!global_io || !global_dbg)
	{
		global_io.reset(new vscode::network(ip, port, rebind));
		global_dbg.reset(new vscode::debugger(global_io.get(), vscode::threadmode::async, global_coding));
		if (launch)
			global_io->kill_process_when_close();
	}
}

void __cdecl debugger_wait_attach()
{
	if (global_dbg) global_dbg->wait_attach();
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
	int coding(lua_State* L)
	{
		const char* str = luaL_checkstring(L, 1);
		if (strcmp(str, "ansi") == 0) {
			debugger_set_coding(0);
		}
		else if (strcmp(str, "utf8") == 0) {
			debugger_set_coding(1);
		}
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

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
			{ "coding", coding },
			{ "listen", listen },
			{ "start", start },
			{ "port", port },
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
