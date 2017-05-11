#pragma once

#if defined(DEBUGGER_DELAYLOAD_LUA)

#include <string>

namespace delayload
{
	void set_luadll(const std::wstring& path);
}

#endif
