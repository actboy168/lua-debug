#include "debugger.h"
#include "dbg_impl.h"
#include "bridge/dbg_delayload.h"

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

void __cdecl debugger_set_luadll(void* luadll, void* getluaapi)
{
#if defined(DEBUGGER_BRIDGE)
	delayload::set_luadll((HMODULE)luadll, (delayload::GetLuaApi)getluaapi);
#endif
}
