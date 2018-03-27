#pragma once

#include "debugger.h"
#include "dbg_protocol.h"

struct lua_State;
class stdinput;

class launch
{
public:
	launch(stdinput& io);
	void update();
	void send(vscode::rprotocol&& rp);
	bool request_launch(vscode::rprotocol& req);

private:
	vscode::debugger debugger_;
	stdinput& io_;
	std::string program_;
	lua_State* launchL_ = 0;
};

