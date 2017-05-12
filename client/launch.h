#pragma once

#include "debugger.h"
#include "dbg_protocol.h"  
#include "dbg_io.h"
#include <net/tcp/buffer.h>

class stdinput;

class launch_io
	: public vscode::io
{
public:
	launch_io(stdinput& io);
	void              update(int ms);
	bool              output(const vscode::wprotocol& wp);
	vscode::rprotocol input();
	bool              input_empty() const;
	void              close();
	void              send(vscode::rprotocol&& rp);

private:
	stdinput& io_;
	net::tcp::buffer<vscode::rprotocol, 8> input_queue_;
};

class launch_server
{
public:
	launch_server(stdinput& io);
	void update();
	void send(vscode::rprotocol&& rp);

private:
	vscode::debugger debugger_;
	launch_io launch_io_;
};

