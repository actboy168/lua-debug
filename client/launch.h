#pragma once

#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_redirect.h"
#include "dbg_protocol.h"  
#include "dbg_io.h"
#include <functional>	
#include <vector>	
#include <net/tcp/buffer.h>

struct lua_State;
class stdinput;

class launch_io
	: public vscode::io
{
public:
	launch_io();
	void              update(int ms);
	bool              output(const vscode::wprotocol& wp);
	vscode::rprotocol input();
	bool              input_empty() const;
	void              close();
	void              send(vscode::rprotocol&& rp);

private:
	bool update_();
	void output_(const char* str, size_t len);

private:
	std::vector<char>                      buffer_;
	net::tcp::buffer<vscode::rprotocol, 8> input_queue_;
};

class launch_server
	: public vscode::custom
{
public:
	launch_server(stdinput& io);
	void update();
	void send(vscode::rprotocol&& rp);

private:
	virtual void set_state(vscode::state state);
	virtual void update_stop();

private:
	vscode::debugger debugger_;
	launch_io launch_io_;
	stdinput& io;
};

