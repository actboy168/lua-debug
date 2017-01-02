#if defined(DEBUGGER_DELAYLOAD_LUA)

#include "dbg_delayload.h"
#include <windows.h>
#include <DelayImp.h>

namespace delayload
{
	static std::wstring lua_dll_;

	void set_lua_dll(const std::wstring& path)
	{
		lua_dll_ = path;
	}

	static FARPROC WINAPI hook(unsigned dliNotify, PDelayLoadInfo pdli)
	{
		switch (dliNotify) {
		case dliStartProcessing:
			break;
		case dliNotePreLoadLibrary:
			if (strcmp("luacore.dll", pdli->szDll) == 0 && !lua_dll_.empty()) {
				return (FARPROC)LoadLibraryW(lua_dll_.c_str());
			}
			break;
		case dliNotePreGetProcAddress:
			break;
		case dliFailLoadLib:
			break;
		case dliFailGetProc:
			break;
		case dliNoteEndProcessing:
			break;
		default:
			return NULL;
		}
		return NULL;
	}
}

#ifndef DELAYIMP_INSECURE_WRITABLE_HOOKS
const
#endif
PfnDliHook __pfnDliNotifyHook2 = delayload::hook;

#endif
