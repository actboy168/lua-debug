#pragma once

#if defined(DEBUGGER_DELAYLOAD_LUA)

#include <string>

namespace delayload
{
	void set_lua_dll(const std::wstring& path);
}

#endif
