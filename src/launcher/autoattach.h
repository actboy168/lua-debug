#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef NDEBUG
#include <stdio.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif
#include "attach_args.h"

namespace autoattach {
	typedef void(*fn_attach)(lua_State* L, attach_args* args);
	void    initialize(fn_attach attach, bool ap);
#ifdef _WIN32
	FARPROC luaapi(const char* name);
#endif
}
