#include <debugger/debugger.h>
#include <debugger/impl.h>
#if defined(DEBUGGER_BRIDGE)
#include <debugger/bridge/delayload.h>
#endif

namespace vscode
{
	debugger::debugger(io::base* io)
		: impl_(new debugger_impl(io))
	{ }

	debugger::~debugger()
	{
		delete impl_;
	}

	bool debugger::open_schema(const std::string& path)
	{
		return impl_->open_schema(path);
	}

	void debugger::close()
	{
		impl_->close();
	}

	void debugger::update()
	{
		impl_->update();
	}

	void debugger::wait_client()
	{
		impl_->wait_client();
	}

	bool debugger::attach_lua(lua_State* L)
	{
		return impl_->attach_lua(L);
	}

	void debugger::detach_lua(lua_State* L, bool remove)
	{
		impl_->detach_lua(L, remove);
	}

	void debugger::set_custom(custom* custom)
	{
		impl_->set_custom(custom);
	}

	void debugger::output(const char* category, const char* buf, size_t len, lua_State* L, lua_Debug* ar)
	{
		impl_->output(category, buf, len, L, (lua::Debug*)ar);
	}

	void debugger::exception(lua_State* L, eException exceptionType, int level)
	{
		impl_->exception(L, exceptionType, level);
	}

	bool debugger::is_state(eState state) const
	{
		return impl_->is_state(state);
	}

	void debugger::open_redirect(eRedirect type, lua_State* L)
	{
		impl_->open_redirect(type, L);
	}

	bool debugger::set_config(int level, const std::string& cfg, std::string& err) 
	{
		return impl_->set_config(level, cfg, err);
	}

	void debugger::init_internal_module(lua_State* L)
	{
		return impl_->init_internal_module(L);
	}
}

void debugger_set_luadll(void* luadll, void* getluaapi)
{
#if defined(DEBUGGER_BRIDGE)
	delayload::set_luadll((HMODULE)luadll, (delayload::GetLuaApi)getluaapi);
#endif
}
