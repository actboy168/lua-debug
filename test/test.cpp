#include <iostream>
#include <thread> 
#include <lua.hpp>	 
#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_hybridarray.h"   
#include "dbg_network.h"
#include "dbg_path.h"
#include "../src/dbg_redirect.h"
#include "../src/dbg_redirect.cpp" 
#include "../src/dbg_unicode.cpp"


#define TEST_ATTACH 1

class debugger_wrapper
	: public vscode::custom
{
public:
	debugger_wrapper(lua_State* L, const char* ip, uint16_t port)
		: network_(ip, port, false)
		, debugger_(&network_, vscode::threadmode::async, vscode::coding::ansi)
	{
		fs::path schema(SOURCE_PATH);
		schema /= "debugProtocol.json";
		network_.set_schema(schema.string().c_str());
		debugger_.attach_lua(L, false);
		debugger_.set_custom(this);
	}

	void update()
	{
		debugger_.update();
	}

	vscode::network  network_;
	vscode::debugger debugger_;
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
