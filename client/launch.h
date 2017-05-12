#pragma once

#include "debugger.h"
#include "dbg_protocol.h"

class stdinput;

class launch
{
public:
	launch(stdinput& io);
	void update();
	void send(vscode::rprotocol&& rp);

private:
	vscode::debugger debugger_;
	stdinput& io_;
};

