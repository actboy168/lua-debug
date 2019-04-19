#if defined(DEBUGGER_BRIDGE)

#include <debugger/bridge/delayload.h>
#include <debugger/bridge/lua.h>
#include <Windows.h>
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#include <DelayImp.h>

namespace delayload {
	static HMODULE luadll_handle = 0;
	static GetLuaApi get_lua_api = ::GetProcAddress;

	void set_luadll(HMODULE handle, GetLuaApi fn) {
		if (luadll_handle) return;
		luadll_handle = handle;
		get_lua_api = fn ? fn : ::GetProcAddress;
	}

    void caller_is_luadll(void* callerAddress) {
        HMODULE  caller = NULL;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)callerAddress, &caller) && caller) {
            if (GetProcAddress(caller, "lua_newstate")) {
                set_luadll(caller, ::GetProcAddress);
            }
        }
    }

	static FARPROC WINAPI hook(unsigned dliNotify, PDelayLoadInfo pdli) {
		switch (dliNotify) {
		case dliNotePreLoadLibrary:
			if (strcmp("lua53.dll", pdli->szDll) == 0) {
                if (luadll_handle) {
					lua::check_version(luadll_handle);
					return (FARPROC)luadll_handle;
				}
			}
			break;
		case dliNotePreGetProcAddress: {
			FARPROC ret = get_lua_api(pdli->hmodCur, pdli->dlp.szProcName);
			if (ret) {
				return ret;
			}
			if (strcmp(pdli->dlp.szProcName, "lua_getuservalue") == 0) {
				lua::lua54::lua_getiuservalue = (int(__cdecl*)(lua_State*, int, int))get_lua_api(pdli->hmodCur, "lua_getiuservalue");
				if (lua::lua54::lua_getiuservalue) {
					return (FARPROC)lua::lua54::lua_getuservalue;
				}
			}
			else if (strcmp(pdli->dlp.szProcName, "lua_setuservalue") == 0) {
				lua::lua54::lua_setiuservalue = (int(__cdecl*)(lua_State*, int, int))get_lua_api(pdli->hmodCur, "lua_setiuservalue");
				if (lua::lua54::lua_setiuservalue) {
					return (FARPROC)lua::lua54::lua_setuservalue;
				}
			}
			else if (strcmp(pdli->dlp.szProcName, "lua_newuserdata") == 0) {
				lua::lua54::lua_newuserdatauv = (void*(__cdecl*)(lua_State*, size_t, int))get_lua_api(pdli->hmodCur, "lua_newuserdatauv");
				if (lua::lua54::lua_newuserdatauv) {
					return (FARPROC)lua::lua54::lua_newuserdata;
				}
			}
			else if (strcmp(pdli->dlp.szProcName, "lua_getprotohash") == 0) {
				return (FARPROC)lua::lua_getprotohash;
			}
			char str[256];
			sprintf(str, "Can't find lua c function: `%s`.", pdli->dlp.szProcName);
			MessageBoxA(0, str, "Fatal Error.", 0);
			return NULL;
		}
			break;
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

#endif
