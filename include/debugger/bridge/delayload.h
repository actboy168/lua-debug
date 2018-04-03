#pragma once

#if defined(DEBUGGER_BRIDGE)

#include <string>
#include <Windows.h>

namespace delayload
{
	typedef FARPROC (__stdcall* GetLuaApi)(HMODULE m, const char* name);

	void set_luadll(const std::wstring& path);
	void set_luadll(HMODULE handle, GetLuaApi fn);
}

#endif
