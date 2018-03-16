#include <thread> 
#include <lua.hpp>	 
#include "debugger.h"
#include "dbg_network.h"

#define STD_SLEEP(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

int main()
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	vscode::network  network("0.0.0.0", 4278, false);
	vscode::debugger debugger(&network, vscode::threadmode::sync, vscode::coding::utf8);
	debugger.attach_lua(L, true);
	for (;;)
	{
		for (int i = 0; i < 100; ++i)
		{
			debugger.update();
			STD_SLEEP(10);
		}

		if (luaL_dostring(L,
			R"(
for i = 1, 10 do
    print('hello', i)
end
			)"))
		{
			printf("%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	return 0;
}
