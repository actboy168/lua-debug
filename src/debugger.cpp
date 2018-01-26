#include "debugger.h"
#include "dbg_impl.h"
#include "dbg_network.h"
#include "dbg_delayload.h"
#include "dbg_unicode.h"

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

	void debugger::set_coding(coding coding)
	{
		impl_->set_coding(coding);
	}

	void debugger::output(const char* category, const char* buf, size_t len)
	{
		impl_->output(category, buf, len, nullptr);
	}
}

std::unique_ptr<vscode::network>  global_io;
std::unique_ptr<vscode::debugger> global_dbg;
vscode::coding                    global_coding = vscode::coding::ansi;

void __cdecl set_luadll(const char* path, size_t len)
{
#if defined(DEBUGGER_DELAYLOAD_LUA)
	delayload::set_luadll(vscode::u2w(vscode::strview(path, len)));
#endif
}

void __cdecl set_coding(int coding)
{
	global_coding = coding == 0 ? vscode::coding::ansi : vscode::coding::utf8;
	if (global_dbg) {
		global_dbg->set_coding(global_coding);
	}
}

uint16_t __cdecl start_server(const char* ip, uint16_t port, bool launch)
{
	if (!global_io || !global_dbg)
	{
		global_io.reset(new vscode::network(ip, port));
		global_dbg.reset(new vscode::debugger(global_io.get(), vscode::threadmode::async, global_coding));
		if (launch)
			global_io->kill_process_when_close();
	}
	return global_io->get_port();
}

void __cdecl attach_lua(lua_State* L, bool pause)
{
	if (global_dbg) global_dbg->attach_lua(L, pause);
}

void __cdecl detach_lua(lua_State* L)
{
	if (global_dbg) global_dbg->detach_lua(L);
}

namespace luaw {
	int coding(lua_State* L)
	{
		const char* str = luaL_checkstring(L, 1);
		if (strcmp(str, "ansi") == 0) {
			::set_coding(0);
		}
		else if (strcmp(str, "utf8") == 0) {
			::set_coding(1);
		}
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int listen(lua_State* L)
	{
		::start_server(luaL_checkstring(L, 1), (uint16_t)luaL_checkinteger(L, 2), false);
		::attach_lua(L, false);
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
	}

	int start(lua_State* L)
	{
		::attach_lua(L, true);
		lua_pushvalue(L, lua_upvalueindex(1));
		return 1;
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
