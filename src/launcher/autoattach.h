#pragma once

#include <Windows.h>

struct lua_State;

namespace autoattach {
	typedef void(*fn_attach)(lua_State* L);
	typedef void(*fn_detach)(lua_State* L);
	void    initialize(fn_attach attach, fn_detach detach, bool ap);
	HMODULE luadll();
}
