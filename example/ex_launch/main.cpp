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
		debugger.update();
		STD_SLEEP(10);
	}
	return 0;
}
