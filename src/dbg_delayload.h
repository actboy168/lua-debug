#pragma once

#if defined(DEBUGGER_DELAYLOAD_LUA)

#include <string>
#include <Windows.h>

namespace delayload
{
	void set_luadll(const std::wstring& path);
	void set_luadll(HMODULE handle);
}

#endif
