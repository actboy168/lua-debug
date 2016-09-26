#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_redirect.h"
#include "dbg_hybridarray.h"
#include <iostream>
#include <thread>
#include <filesystem>  
#include <lua.hpp>

namespace fs = std::tr2::sys;

#define TEST_ATTACH 1

class debugger_wrapper
	: public vscode::custom
{
public:
	debugger_wrapper(lua_State* L, const char* ip, uint16_t port)
		: debugger_(L, ip, port)
	{
		fs::path schema(SOURCE_PATH);
		schema /= "debugProtocol.json";
		debugger_.set_schema(schema.string().c_str());
		debugger_.set_custom(this);
	}

	virtual void set_state(vscode::state state)
	{
		state_ = state;
		switch (state)
		{
		case vscode::state::initialized:
			redirect_.open();
			break;
		case vscode::state::terminated:
			redirect_.close();
			break;
		default:
			break;
		}
	}

	virtual void update_stop()
	{
		update_redirect();
	}

	void update()
	{
		update_redirect();
		debugger_.update();
	}

	void update_redirect()
	{
		if (state_ == vscode::state::birth)
			return;

		size_t n = 0;
		n = redirect_.peek_stdout();
		if (n > 0)
		{
			vscode::hybridarray<char, 1024> buf(n);
			redirect_.read_stdout(buf.data(), buf.size());
			debugger_.output("stdout", buf.data(), buf.size());
		}
		n = redirect_.peek_stdout();
		if (n > 0)
		{
			vscode::hybridarray<char, 1024> buf(n);
			redirect_.read_stdout(buf.data(), buf.size());
			debugger_.output("stderr", buf.data(), buf.size());
		}
	}

	vscode::debugger debugger_;
	vscode::state state_;
	vscode::redirector redirect_;
};

int main()
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	try {
		fs::path cwd(SOURCE_PATH);
		fs::current_path(cwd);

		debugger_wrapper dbg(L, "0.0.0.0", 4278);;

		for (size_t n = 0
			;
			; std::this_thread::sleep_for(std::chrono::milliseconds(10))
			, n++)
		{
			dbg.update();

#if TEST_ATTACH
			if (n % 100)
			{
				if (luaL_dofile(L, "main.lua"))
				{
					std::cerr << lua_tostring(L, -1) << std::endl;
					lua_pop(L, 1);
				}
			}
#endif
		}
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
