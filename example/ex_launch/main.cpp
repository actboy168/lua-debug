#include <thread> 
#include <lua.hpp>	 
#include <debugger/debugger.h>
#include <debugger/io/socket.h>

#define STD_SLEEP(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

int main()
{
	vscode::io::socket network("0.0.0.0", 4278, false);
	vscode::debugger   debugger(&network, vscode::threadmode::sync);
	for (;;)
	{
		debugger.update();
		STD_SLEEP(10);
	}
	return 0;
}
