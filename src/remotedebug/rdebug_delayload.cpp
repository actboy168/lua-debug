#if defined(_MSC_VER)

#include "rdebug_delayload.h"
#include <lua.hpp>
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#include <DelayImp.h>

#if LUA_VERSION_NUM == 504
#define LUA_DLL_NAME "lua54.dll"
#elif LUA_VERSION_NUM == 503
#define LUA_DLL_NAME "lua53.dll"
#else
#error "Unknown Lua Version: " #LUA_VERSION_NUM
#endif

namespace remotedebug::delayload {
	static HMODULE luadll_handle = 0;

	void set_luadll(HMODULE handle) {
		if (luadll_handle) return;
		luadll_handle = handle;
	}

	void caller_is_luadll(void* callerAddress) {
		if (luadll_handle) return;
		HMODULE caller = NULL;
		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)callerAddress, &caller) && caller) {
			if (GetProcAddress(caller, "lua_newstate")) {
				set_luadll(caller);
			}
		}
	}

	static FARPROC WINAPI hook(unsigned dliNotify, PDelayLoadInfo pdli) {
		switch (dliNotify) {
		case dliNotePreLoadLibrary:
			if (strcmp(LUA_DLL_NAME, pdli->szDll) == 0) {
				if (luadll_handle) {
					return (FARPROC)luadll_handle;
				}
			}
			return NULL;
		case dliNotePreGetProcAddress: {
			FARPROC ret = ::GetProcAddress(pdli->hmodCur, pdli->dlp.szProcName);
			if (ret) {
				return ret;
			}
			char str[256];
			sprintf(str, "Can't find lua c function: `%s`.", pdli->dlp.szProcName);
			MessageBoxA(0, str, "Fatal Error.", 0);
			return NULL;
		}
		case dliStartProcessing:
		case dliFailLoadLib:
		case dliFailGetProc:
		case dliNoteEndProcessing:
		default:
			return NULL;
		}
		return NULL;
	}
}

PfnDliHook __pfnDliNotifyHook2 = remotedebug::delayload::hook;

#endif
