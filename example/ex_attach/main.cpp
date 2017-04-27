#include <thread> 
#include <lua.hpp>	 
#include "debugger.h"
#include "dbg_network.h"

#define STD_SLEEP(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

int main()
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	vscode::network  network("0.0.0.0", 4278);
	vscode::debugger debugger(L, &network);
	for (;;)
	{
		for (int i = 0; i < 100; ++i)
		{
			debugger.update();
			STD_SLEEP(10);
		}

		if (luaL_dostring(L,
			R"(
				local a = 1
				local b = 2
				print(a, b)
				print('ok')
			)"))
		{
			printf("%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	return 0;
}
