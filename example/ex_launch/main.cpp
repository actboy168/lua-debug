#include <thread> 
#include <lua.hpp>	 
#include "debugger.h"
#include "dbg_network.h"

#define STD_SLEEP(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

int main()
{
	vscode::network  network("0.0.0.0", 4278, false);
	vscode::debugger debugger(&network, vscode::threadmode::sync, vscode::coding::utf8);
	for (;;)
	{
		debugger.update();
		STD_SLEEP(10);
	}
	return 0;
}
