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

	void debugger::output(const char* category, const char* buf, size_t len)
	{
		impl_->output(category, buf, len);
	}
}

struct c_debugger {
	vscode::network  io;
	vscode::debugger dbg;

	c_debugger(const char* ip, uint16_t port, vscode::threadmode mode)
		: io(ip, port)
		, dbg(&io, mode)
	{ }
};

void* __cdecl vscode_debugger_create(const char* ip, uint16_t port, int threadmode)
{
	void* dbg = malloc(sizeof c_debugger);
	if (!dbg) {
		return 0;
	}
	new (dbg) c_debugger(ip, port, (vscode::threadmode)threadmode);
	return dbg;
}

void __cdecl vscode_debugger_close(void* dbg)
{
	((c_debugger*)dbg)->~c_debugger();
	free((c_debugger*)dbg);
}

void __cdecl vscode_debugger_update(void* dbg)
{
	((c_debugger*)dbg)->dbg.update();
}

void  __cdecl vscode_debugger_attach_lua(void* dbg, lua_State* L)
{
	((c_debugger*)dbg)->dbg.attach_lua(L);
}

void  __cdecl vscode_debugger_detach_lua(void* dbg, lua_State* L)
{
	((c_debugger*)dbg)->dbg.detach_lua(L);
}
