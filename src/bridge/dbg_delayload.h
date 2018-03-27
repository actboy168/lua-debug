#pragma once

#if defined(DEBUGGER_BRIDGE)

#include <string>
#include <Windows.h>

namespace delayload
{
	void set_luadll(const std::wstring& path);
	void set_luadll(HMODULE handle);
}

#endif
