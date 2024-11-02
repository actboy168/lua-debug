#if defined(_WIN32)

#    include "rdebug_win32.h"

#    include <Windows.h>
#    include <stdint.h>

#    include <string>

namespace luadebug::win32 {
    typedef FARPROC (*FindLuaApi)(const char* name);
    static HMODULE luadll    = 0;
    static FindLuaApi luaapi = 0;
    static bool debugSelf    = false;

    HMODULE get_luadll() {
        return luadll;
    }

    void set_luadll(HMODULE handle) {
        if (luadll) return;
        luadll = handle;
    }

    void set_luaapi(void* fn) {
        if (luaapi) return;
        luaapi = (FindLuaApi)fn;
    }

    void setflag_debugself(bool flag) {
        if (debugSelf == flag) return;
        debugSelf = flag;
        // set lua dll self
        caller_is_luadll(&set_luaapi);
        // set lua api finder
        luaapi = +[](const char* name) {
            auto n = "luadbg" + std::string { name }.substr(3);
            return ::GetProcAddress(luadll, n.c_str());
        };
    }

    const char* get_lua_module_key() {
        return debugSelf ? "luadbg_newstate" : "lua_newstate";
    }

    bool caller_is_luadll(void* callerAddress) {
        if (luadll) return true;
        HMODULE caller = NULL;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)callerAddress, &caller) && caller) {
            if (GetProcAddress(caller, get_lua_module_key())) {
                set_luadll(caller);
                return true;
            }
        }
        return false;
    }

    static HMODULE enumerate_modules(HANDLE hProcess, HMODULE hModuleLast, PIMAGE_NT_HEADERS32 pNtHeader) {
        MEMORY_BASIC_INFORMATION mbi = { 0 };
        for (PBYTE pbLast = (PBYTE)hModuleLast + 0x10000;; pbLast = (PBYTE)mbi.BaseAddress + mbi.RegionSize) {
            if (VirtualQueryEx(hProcess, (PVOID)pbLast, &mbi, sizeof(mbi)) <= 0) {
                break;
            }
            if (((PBYTE)mbi.BaseAddress + mbi.RegionSize) < pbLast) {
                break;
            }
            if ((mbi.State != MEM_COMMIT) ||
                ((mbi.Protect & 0xff) == PAGE_NOACCESS) ||
                (mbi.Protect & PAGE_GUARD)) {
                continue;
            }
            __try {
                IMAGE_DOS_HEADER idh;
                if (!ReadProcessMemory(hProcess, pbLast, &idh, sizeof(idh), NULL)) {
                    continue;
                }
                if (idh.e_magic != IMAGE_DOS_SIGNATURE || (DWORD)idh.e_lfanew > mbi.RegionSize || (DWORD)idh.e_lfanew < sizeof(idh)) {
                    continue;
                }
                if (!ReadProcessMemory(hProcess, pbLast + idh.e_lfanew, pNtHeader, sizeof(*pNtHeader), NULL)) {
                    continue;
                }
                if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
                    continue;
                }
                return (HMODULE)pbLast;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                continue;
            }
        }
        return NULL;
    }

    bool find_luadll() {
        if (luadll) return true;
        HANDLE hProcess = GetCurrentProcess();
        HMODULE hModule = NULL;
        for (;;) {
            IMAGE_NT_HEADERS32 inh;
            if ((hModule = enumerate_modules(hProcess, hModule, &inh)) == NULL) {
                break;
            }
            if (GetProcAddress(hModule, get_lua_module_key())) {
                set_luadll(hModule);
                return true;
            }
        }
        return false;
    }

    static uintptr_t rva_to_addr(HMODULE module, uintptr_t rva) {
        if (rva == 0) return 0;
        return (uintptr_t)module + rva;
    }
    static uintptr_t find_putenv() {
        HMODULE module = get_luadll();
        if (!module) {
            return 0;
        }
        PIMAGE_DOS_HEADER dos_header    = (PIMAGE_DOS_HEADER)(module);
        PIMAGE_NT_HEADERS nt_headers    = (PIMAGE_NT_HEADERS)((uintptr_t)(dos_header) + dos_header->e_lfanew);
        PIMAGE_IMPORT_DESCRIPTOR import = (PIMAGE_IMPORT_DESCRIPTOR)rva_to_addr(module, nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        uint32_t size                   = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
        if (import == NULL || size < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
            return 0;
        }
        for (; import->FirstThunk; ++import) {
            PIMAGE_THUNK_DATA pitd  = (PIMAGE_THUNK_DATA)rva_to_addr(module, import->OriginalFirstThunk);
            PIMAGE_THUNK_DATA pitd2 = (PIMAGE_THUNK_DATA)rva_to_addr(module, import->FirstThunk);
            for (; pitd->u1.Function; ++pitd, ++pitd2) {
                PIMAGE_IMPORT_BY_NAME pi_import_by_name = (PIMAGE_IMPORT_BY_NAME)(rva_to_addr(module, *(uintptr_t*)pitd));
                if (!IMAGE_SNAP_BY_ORDINAL(pitd->u1.Ordinal)) {
                    const char* apiname = (const char*)pi_import_by_name->Name;
                    if (0 == strcmp(apiname, "getenv") || 0 == strcmp(apiname, "_wgetenv")) {
                        HMODULE crt = GetModuleHandleA((const char*)rva_to_addr(module, import->Name));
                        if (crt) {
                            return (uintptr_t)GetProcAddress(crt, "_putenv");
                        }
                    }
                }
            }
        }
        return 0;
    }

    void putenv(const char* envstr) {
        static auto lua_putenv = (int(__cdecl*)(const char*))find_putenv();
        if (lua_putenv) {
            lua_putenv(envstr);
        } else {
            ::_putenv(envstr);
        }
    }
}

#    if defined(_MSC_VER)
#        include <lua.hpp>
#        define DELAYIMP_INSECURE_WRITABLE_HOOKS
#        include <DelayImp.h>

#        if !defined(LUA_DLL_VERSION)
#            error "Need LUA_DLL_VERSION"
#        endif
// clang-format off
#    define LUA_STRINGIZE(_x) LUA_STRINGIZE_(_x)
#    define LUA_STRINGIZE_(_x) #_x
// clang-format on

#        define LUA_DLL_NAME LUA_STRINGIZE(LUA_DLL_VERSION) ".dll"

namespace luadebug::win32 {
    static int (*_lua_pcall)(lua_State* L, int nargs, int nresults, int errfunc);
    static int _lua_pcallk(lua_State* L, int nargs, int nresults, int errfunc, intptr_t ctx, intptr_t k) {
        return _lua_pcall(L, nargs, nresults, errfunc);
    }

    static int (*_luaL_loadbuffer)(lua_State* L, const char* buff, size_t size, const char* name);
    static int _luaL_loadbufferx(lua_State* L, const char* buff, size_t size, const char* name, const char* mode) {
        return _luaL_loadbuffer(L, buff, size, name);
    }

    static FARPROC WINAPI delayload_hook(unsigned dliNotify, PDelayLoadInfo pdli) {
        switch (dliNotify) {
        case dliNotePreLoadLibrary:
            if (strcmp(LUA_DLL_NAME, pdli->szDll) == 0) {
                if (luadll) {
                    return (FARPROC)luadll;
                }
            }
            return NULL;
        case dliNotePreGetProcAddress: {
            if (luaapi) {
                FARPROC fn = luaapi(pdli->dlp.szProcName);
                if (fn) {
                    return fn;
                }
            }
            FARPROC ret = ::GetProcAddress(pdli->hmodCur, pdli->dlp.szProcName);
            if (ret) {
                return ret;
            }
            if (strcmp(pdli->dlp.szProcName, "lua_pcallk") == 0) {
                _lua_pcall = (int(__cdecl*)(lua_State*, int, int, int))::GetProcAddress(pdli->hmodCur, "lua_pcall");
                if (_lua_pcall) {
                    return (FARPROC)_lua_pcallk;
                }
            } else if (strcmp(pdli->dlp.szProcName, "luaL_loadbufferx") == 0) {
                _luaL_loadbuffer = (int(__cdecl*)(lua_State*, const char*, size_t, const char*))::GetProcAddress(pdli->hmodCur, "luaL_loadbuffer");
                if (_luaL_loadbuffer) {
                    return (FARPROC)_luaL_loadbufferx;
                }
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

PfnDliHook __pfnDliNotifyHook2 = luadebug::win32::delayload_hook;

#    endif

#endif
