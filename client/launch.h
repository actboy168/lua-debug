#pragma once

#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_redirect.h"
#include "dbg_protocol.h"  
#include "dbg_io.h"
#include <lua.hpp>
#include <functional>	
#include <vector>	
#include <net/tcp/buffer.h>

class launch_io
	: public vscode::io
{
public:
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
	launch_server(std::function<void()> idle);
	void update();
	void send(vscode::rprotocol&& rp);

private:
	lua_State* initLua();
	virtual void set_state(vscode::state state);
	virtual void update_stop();
	void         update_redirect();
	static int   print(lua_State *L);

private:
	lua_State *L;
	vscode::debugger debugger_;
	vscode::state state_;
	vscode::redirector stderr_;
	std::function<void()> idle_;
	launch_io io_;
};

