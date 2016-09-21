#include "debugger.h"
#include <iostream>
#include <thread>
#include <filesystem>  
#include <lua.hpp>

namespace fs = std::tr2::sys;

#define TEST_ATTACH 0

int main()
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	try {
		fs::path cwd(SOURCE_PATH);
		cwd /= "test";
		fs::current_path(cwd);

		vscode::debugger dbg(L, "0.0.0.0", 4278);
		cwd /= "debugProtocol.json";
		dbg.set_schema(cwd.string().c_str());

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
