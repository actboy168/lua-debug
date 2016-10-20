#include "launch.h"
#include "dbg_hybridarray.h"
#include "dbg_format.h"	 
#include "dbg_unicode.h"
#include <lua.hpp>
#include <Windows.h>

template <class T>
static std::wstring a2w(const T& str)
{
	if (str.empty())
	{
		return L"";
	}
	int wlen = ::MultiByteToWideChar(CP_ACP, 0, str.data(), str.size(), NULL, 0);
	if (wlen <= 0)
	{
		return L"";
	}
	std::vector<wchar_t> result(wlen);
	::MultiByteToWideChar(CP_ACP, 0, str.data(), str.size(), result.data(), wlen);
	return std::wstring(result.data(), result.size());
}

template <class T>
static std::string a2u(const T& str)
{
	return vscode::w2u(a2w(str));
}

void launch_io::update(int ms)
{
}

void launch_io::output_(const char* str, size_t len) {
	for (;;) {
		size_t r = fwrite(str, len, 1, stdout);
		if (r == 1)
			break;
	}
}

bool launch_io::output(const vscode::wprotocol& wp)
{
	if (!wp.IsComplete())
		return false;
	auto l = vscode::format("Content-Length: %d\r\n\r\n", wp.size());
	output_(l.data(), l.size());
	output_(wp.data(), wp.size());
	return true;
}

vscode::rprotocol launch_io::input()
{
	if (input_queue_.empty())
		return vscode::rprotocol();
	vscode::rprotocol r = std::move(input_queue_.front());
	input_queue_.pop();
	return r;
}

bool launch_io::input_empty() const
{
	return input_queue_.empty();
}

void launch_io::send(vscode::rprotocol&& rp)
{
	input_queue_.push(std::forward<vscode::rprotocol>(rp));
}

void launch_io::close()
{
	exit(0);
}

launch_server::launch_server(const std::string& console, std::function<void()> idle)
	: L(initLua())
	, io_()
	, debugger_(L, &io_)
	, idle_(idle)
	, console_(encoding::none)
{
	debugger_.set_custom(this);

	if (console == "ansi") {
		console_ = encoding::ansi;
	}
	else if (console == "utf8") {
		console_ = encoding::utf8;
	}
	else {
		return;
	}
	lua_pushlightuserdata(L, this);
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
	launch_server* srv = (launch_server*)lua_touserdata(L, lua_upvalueindex(1));
	srv->output("stdout", out);
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
	if (console_ == encoding::none)
		return;
	size_t n = stderr_.peek();
	if (n > 0)
	{
		vscode::hybridarray<char, 1024> buf(n);
		stderr_.read(buf.data(), buf.size());
		if (console_ == encoding::ansi) {
			std::string utf8 = a2u(buf);
			output("stderr", utf8);
		}
		else {
			output("stderr", buf);
		}
	}
}

void launch_server::send(vscode::rprotocol&& rp)
{
	io_.send(std::forward<vscode::rprotocol>(rp));
}

void launch_server::output(const char* category, const strview& str)
{
	if (console_ == encoding::ansi) {
		std::string utf8 = a2u(str);
		debugger_.output(category, utf8.data(), utf8.size());
	}
	else {
		debugger_.output(category, str.data(), str.size());
	}
}
