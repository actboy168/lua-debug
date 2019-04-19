#pragma once

#if defined(DEBUGGER_BRIDGE)

#include <string>
#include <Windows.h>

namespace delayload {
	typedef FARPROC (__stdcall* GetLuaApi)(HMODULE m, const char* name);
	void set_luadll(HMODULE handle, GetLuaApi fn);
    void caller_is_luadll(void* callerAddress);
}

#endif
