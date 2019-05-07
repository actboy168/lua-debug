#include "delayload.h"
#include <lua.hpp>
#include <assert.h>
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#include <DelayImp.h>

namespace delayload {
	static HMODULE luadll_handle = 0;

	void set_luadll(HMODULE handle) {
		if (luadll_handle) return;
		luadll_handle = handle;
	}

	static FARPROC WINAPI hook(unsigned dliNotify, PDelayLoadInfo pdli) {
		switch (dliNotify) {
		case dliNotePreLoadLibrary:
			if (strcmp("lua54.dll", pdli->szDll) == 0) {
				assert(luadll_handle);
				return (FARPROC)luadll_handle;
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

PfnDliHook __pfnDliNotifyHook2 = delayload::hook;
