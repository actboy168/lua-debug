#include "debugger.h"
#include "dbg_impl.h"

namespace vscode
{
	debugger::debugger(lua_State* L, const char* ip, uint16_t port)
		: impl_(new debugger_impl(L, ip, port))
	{ }

	debugger::~debugger()
	{
		delete impl_;
	}

	void debugger::update()
	{
		impl_->update();
	}

	void debugger::set_schema(const char* file)
	{
		impl_->set_schema(file);
	}

	void debugger::set_custom(custom* custom)
	{
		impl_->set_custom(custom);
	}
}

void* __cdecl vscode_debugger_create(lua_State* L, const char* ip, uint16_t port)
{
	void* dbg = malloc(sizeof vscode::debugger);
	if (!dbg) {
		return 0;
	}
	new (dbg)vscode::debugger(L, ip, port);
	return dbg;
}

void __cdecl vscode_debugger_close(void* dbg)
{
	free((vscode::debugger*)dbg);
}

void __cdecl vscode_debugger_set_schema(void* dbg, const char* file)
{
	((vscode::debugger*)dbg)->set_schema(file);
}

void __cdecl vscode_debugger_update(void* dbg)
{
	((vscode::debugger*)dbg)->update();
}
