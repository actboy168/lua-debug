#if defined(DEBUGGER_DELAYLOAD_LUA)

#include "dbg_delayload.h"
#include <windows.h>
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#include <DelayImp.h>

namespace delayload
{
	static std::wstring luadll_path;
	static HMODULE luadll_handle = 0;

	void set_luadll(HMODULE handle)
	{
		luadll_handle = handle;
	}

	void set_luadll(const std::wstring& path)
	{
		luadll_path = path;
	}

	static FARPROC WINAPI hook(unsigned dliNotify, PDelayLoadInfo pdli)
	{
		switch (dliNotify) {
		case dliStartProcessing:
			break;
		case dliNotePreLoadLibrary:
			if (strcmp("luacore.dll", pdli->szDll) == 0) {
				if (!luadll_path.empty()) {
					return (FARPROC)LoadLibraryW(luadll_path.c_str());
				}
				else if (luadll_handle) {
					return (FARPROC)luadll_handle;
				}
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

PfnDliHook __pfnDliNotifyHook2 = delayload::hook;

#endif
