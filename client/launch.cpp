#include "launch.h"
#include "dbg_hybridarray.h"

launch_server::launch_server(const char* ip, uint16_t port, std::function<void()> idle)
	: L(initLua())
	, debugger_(L, ip, port)
	, idle_(idle)
{
	debugger_.set_custom(this);

	lua_pushlightuserdata(L, &debugger_);
	lua_pushcclosure(L, print, 1);
	lua_setglobal(L, "print");
}

int launch_server::print(lua_State *L) {
	std::string out;
	int n = lua_gettop(L);
	int i;
	lua_getglobal(L, "tostring");
	for (i = 1; i <= n; i++) {
		const char *s;
		size_t l;
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &l);
		if (s == NULL)
			return luaL_error(L, "'tostring' must return a string to 'print'");
		if (i>1) out += "\t";
		out += std::string(s, l);
		lua_pop(L, 1);
	}
	out += "\n";
	vscode::debugger* dbg = (vscode::debugger*)lua_touserdata(L, lua_upvalueindex(1));
	dbg->output("stdout", out.data(), out.size());
	return 0;
}

lua_State* launch_server::initLua()
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	return L;
}

void launch_server::set_state(vscode::state state)
{
	state_ = state;
	switch (state)
	{
	case vscode::state::initialized:
		stderr_.open("stderr", vscode::std_fd::STDERR);
		break;
	case vscode::state::terminated:
		stderr_.close();
		break;
	default:
		break;
	}
}

void launch_server::update_stop()
{
	update_redirect();
	idle_();
}

void launch_server::update()
{
	update_redirect();
	debugger_.update();
}

void launch_server::update_redirect()
{
	if (state_ == vscode::state::birth)
		return;
	size_t n = stderr_.peek();
	if (n > 0)
	{
		vscode::hybridarray<char, 1024> buf(n);
		stderr_.read(buf.data(), buf.size());
		debugger_.output("stderr", buf.data(), buf.size());
	}
}

