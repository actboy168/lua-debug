#pragma once

#include "debugger.h"
#include "dbg_custom.h"
#include "dbg_redirect.h"
#include "dbg_protocol.h"
#include <lua.hpp>
#include <functional>

class launch_server
	: public vscode::custom
{
public:
	launch_server(vscode::rprotocol& proto, const char* ip, uint16_t port, std::function<void()> idle);
	void update();

private:
	lua_State* initLua(vscode::rprotocol& proto);
	virtual void set_state(vscode::state state);
	virtual void update_stop();
	void update_redirect();
	static int print(lua_State *L);

private:
	lua_State *L;
	vscode::debugger debugger_;
	vscode::state state_;
	vscode::redirector stderr_;
	std::function<void()> idle_;
};

