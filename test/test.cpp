#include <iostream>
#include <thread> 
#include <lua.hpp>	 
#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_hybridarray.h"   
#include "dbg_network.h"
#include "dbg_path.h"
#include "../client/dbg_redirect.h"	 
#include "../client/dbg_redirect.cpp"


#define TEST_ATTACH 1

class debugger_wrapper
	: public vscode::custom
{
public:
	debugger_wrapper(lua_State* L, const char* ip, uint16_t port)
		: network_(ip, port)
		, debugger_(L, &network_)
	{
		fs::path schema(SOURCE_PATH);
		schema /= "debugProtocol.json";
		network_.set_schema(schema.string().c_str());
		debugger_.set_custom(this);
	}

	virtual void set_state(vscode::state state)
	{
		state_ = state;
		switch (state)
		{
		case vscode::state::initialized:
			stdout_.open("stdout", vscode::std_fd::STDOUT);
			stderr_.open("stderr", vscode::std_fd::STDERR);
			break;
		case vscode::state::terminated:
			stdout_.close();
			stderr_.close();
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
		n = stdout_.peek();
		if (n > 0)
		{
			vscode::hybridarray<char, 1024> buf(n);
			stdout_.read(buf.data(), buf.size());
			debugger_.output("stdout", buf.data(), buf.size());
		}
		n = stderr_.peek();
		if (n > 0)
		{
			vscode::hybridarray<char, 1024> buf(n);
			stderr_.read(buf.data(), buf.size());
			debugger_.output("stderr", buf.data(), buf.size());
		}
	}

	vscode::network  network_;
	vscode::debugger debugger_;
	vscode::state state_;
	vscode::redirector stdout_;
	vscode::redirector stderr_;
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
