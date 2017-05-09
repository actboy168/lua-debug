#include "debugger.h"
#include "dbg_impl.h"
#include "dbg_network.h"

namespace vscode
{
	debugger::debugger(io* io)
		: impl_(new debugger_impl(io))
	{ }

	debugger::~debugger()
	{
		delete impl_;
	}

	void debugger::update()
	{
		impl_->update();
	}

	void debugger::set_lua(lua_State* L)
	{
		impl_->set_lua(L);
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

	c_debugger(lua_State* L, const char* ip, uint16_t port)
		: io(ip, port)
		, dbg( &io)
	{
		dbg.set_lua(L);
	}
};

void* __cdecl vscode_debugger_create(lua_State* L, const char* ip, uint16_t port)
{
	void* dbg = malloc(sizeof c_debugger);
	if (!dbg) {
		return 0;
	}
	new (dbg) c_debugger(L, ip, port);
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
