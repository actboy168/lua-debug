#include "autoattach.h"
#include <lua.hpp>
#include <mutex>
#include <stack>
#include <set>
#include <vector>

#if defined(_WIN32)
#include <intrin.h>
#include <detours.h>
#include "fp_call.h"
#include "../remotedebug/rdebug_delayload.h"
#else
#include <dobby.h>
#include <dlfcn.h>
using HMODULE = void*;
#define __cdecl 
typedef struct _RuntimeModule {
	char path[1024];
	void *load_address;
} RuntimeModule;
#endif

namespace autoattach {
	std::mutex lockLoadDll;
	std::set<lua_State*> hookLuaStates;
	fn_attach debuggerAttach;
	bool      attachProcess = false;
	HMODULE   hookDll = NULL;

#if defined(_WIN32)
	static bool hook_install(uintptr_t* pointer_ptr, uintptr_t detour) {
		LONG status;
		if ((status = DetourTransactionBegin()) == NO_ERROR) {
			if ((status = DetourUpdateThread(::GetCurrentThread())) == NO_ERROR) {
				if ((status = DetourAttach((PVOID*)pointer_ptr, (PVOID)detour)) == NO_ERROR) {
					if ((status = DetourTransactionCommit()) == NO_ERROR) {
						return true;
					}
				}
			}
			DetourTransactionAbort();
		}
		::SetLastError(status);
		return false;
	}

	static bool hook_uninstall(uintptr_t* pointer_ptr, uintptr_t detour) {
		LONG status;
		if ((status = DetourTransactionBegin()) == NO_ERROR) {
			if ((status = DetourUpdateThread(::GetCurrentThread())) == NO_ERROR) {
				if ((status = DetourDetach((PVOID*)pointer_ptr, (PVOID)detour)) == NO_ERROR) {
					if ((status = DetourTransactionCommit()) == NO_ERROR) {
						return true;
					}
				}
			}
			DetourTransactionAbort();
		}
		::SetLastError(status);
		return false;
	}
#endif

	namespace lua {
		namespace real {
			uintptr_t luaL_openlibs = 0;
			uintptr_t lua_close = 0;
			uintptr_t lua_settop = 0;
		}
#if defined(_WIN32)
		namespace fake_launch {
			static void __cdecl luaL_openlibs(lua_State* L) {
				base::c_call<void>(real::luaL_openlibs, L);
				debuggerAttach(L);
			}
		}
		namespace fake_attach {
			static void __cdecl luaL_openlibs(lua_State* L) {
				base::c_call<void>(real::luaL_openlibs, L);
				if (hookLuaStates.insert(L).second) {
					debuggerAttach(L);
				}
			}
			static void __cdecl lua_settop(lua_State *L, int index) {
				if (hookLuaStates.insert(L).second) {
					debuggerAttach(L);
				}
				return base::c_call<void>(real::lua_settop, L, index);
			}
			static void __cdecl lua_close(lua_State* L) {
				base::c_call<void>(real::lua_close, L);
				hookLuaStates.erase(L);
			}
		}

		bool hook(HMODULE m) {
			struct Hook {
				uintptr_t& real;
				uintptr_t fake;
			};
			std::vector<Hook> tasks;

#define HOOK(type, name) do {\
			if (!real::##name) { \
				real::##name = (uintptr_t)GetProcAddress(m, #name); \
				if (!real::##name) return false; \
				tasks.push_back({real::##name, (uintptr_t)fake_##type##::##name}); \
			} \
		} while (0)

			if (attachProcess) {
				HOOK(attach, luaL_openlibs);
				HOOK(attach, lua_close);
				HOOK(attach, lua_settop);
			}
			else {
				HOOK(launch, luaL_openlibs);
			}

			for (size_t i = 0; i < tasks.size(); ++i) {
				if (!hook_install(&tasks[i].real, tasks[i].fake)) {
					for (ptrdiff_t j = i - 1; j >= 0; ++j) {
						hook_uninstall(&tasks[j].real, tasks[j].fake);
					}
					return false;
				}
			}
			return true;
		}

#else
        namespace original {
			uintptr_t luaL_openlibs = 0;
			uintptr_t lua_close = 0;
			uintptr_t lua_settop = 0;  
        }
        namespace fake_launch {
			static void luaL_openlibs(lua_State* L) {
                ((decltype(&::luaL_openlibs))real::luaL_openlibs)(L);
				debuggerAttach(L);
			}
		}
		namespace fake_attach {
			static void luaL_openlibs(lua_State* L) {
                ((decltype(&::luaL_openlibs))real::luaL_openlibs)(L);
				if (hookLuaStates.insert(L).second) {
					debuggerAttach(L);
				}
			}
			static void lua_settop(lua_State *L, int index) {
				if (hookLuaStates.insert(L).second) {
					debuggerAttach(L);
				}

                return ((decltype(&::lua_settop))real::lua_settop)(L, index);
			}
			static void lua_close(lua_State* L) {
                ((decltype(&::lua_close))real::lua_close)(L);
				hookLuaStates.erase(L);
			}
		}

		bool hook(const RuntimeModule& module){
			intptr_t luaL_openlibs_ptr = (intptr_t)DobbySymbolResolver(module.path, "luaL_openlibs");
            intptr_t lua_settop_ptr = (intptr_t)DobbySymbolResolver(module.path, "lua_settop");
            intptr_t lua_close_ptr = (intptr_t)DobbySymbolResolver(module.path, "lua_close");
            if (!(luaL_openlibs_ptr && lua_settop_ptr && lua_close_ptr))
				return false;
            do {
				if (attachProcess) {
					if (DobbyHook((void*)luaL_openlibs_ptr, (dobby_dummy_func_t)&lua::fake_attach::luaL_openlibs, (dobby_dummy_func_t*)&lua::real::luaL_openlibs) != 0)
						break;
					if (DobbyHook((void*)lua_settop_ptr, (dobby_dummy_func_t)&lua::fake_attach::lua_settop, (dobby_dummy_func_t*)&lua::real::lua_settop) != 0)
						break;
					if (DobbyHook((void*)lua_close_ptr, (dobby_dummy_func_t)&lua::fake_attach::lua_close, (dobby_dummy_func_t*)&lua::real::lua_close) != 0)
						break;
				} else {
					if (DobbyHook((void*)luaL_openlibs_ptr, (dobby_dummy_func_t)&lua::fake_launch::luaL_openlibs, (dobby_dummy_func_t*)&lua::real::luaL_openlibs) != 0)
						break;
				}

				lua::original::luaL_openlibs = luaL_openlibs_ptr;
				lua::original::lua_settop = lua_settop_ptr;
				lua::original::lua_close = lua_close_ptr;
				return true;
			} while (0);
			DobbyDestroy((void*)luaL_openlibs_ptr);
			DobbyDestroy((void*)lua_settop_ptr);
			DobbyDestroy((void*)lua_close_ptr);
			return false;
		}
#endif
    }

#if defined(_WIN32)
	std::set<std::wstring> loadedModules;

	static HMODULE enumerateModules(HANDLE hProcess, HMODULE hModuleLast, PIMAGE_NT_HEADERS32 pNtHeader) {
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
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				continue;
			}
		}
		return NULL;
	}

	static bool tryHookLuaDll(HMODULE hModule) {
		if (hookDll) {
			return true;
		}
		wchar_t moduleName[MAX_PATH];
		GetModuleFileNameW(hModule, moduleName, MAX_PATH);
		if (loadedModules.find(moduleName) != loadedModules.end()) {
			return false;
		}
		loadedModules.insert(moduleName);
		if (!lua::hook(hModule)) {
			return false;
		}
		hookDll = hModule;
		remotedebug::delayload::set_luadll(hModule);
		remotedebug::delayload::set_luaapi(luaapi);
		return true;
	}

	static bool findLuaDll() {
		std::unique_lock<std::mutex> lock(lockLoadDll);
		HANDLE hProcess = GetCurrentProcess();
		HMODULE hModule = NULL;
		for (;;) {
			IMAGE_NT_HEADERS32 inh;
			if ((hModule = enumerateModules(hProcess, hModule, &inh)) == NULL) {
				break;
			}
			if (tryHookLuaDll(hModule)) {
				return true;
			}
		}
		return false;
	}

	uintptr_t realLoadLibraryExW = 0;
	HMODULE __stdcall fakeLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) {
		HMODULE hModule = base::std_call<HMODULE>(realLoadLibraryExW, lpLibFileName, hFile, dwFlags);
		std::unique_lock<std::mutex> lock(lockLoadDll);
		tryHookLuaDll(hModule);
		return hModule;
	}

	static void waitLuaDll() {
		HMODULE hModuleKernel = GetModuleHandleW(L"kernel32.dll");
		if (hModuleKernel) {
			realLoadLibraryExW = (uintptr_t)GetProcAddress(hModuleKernel, "LoadLibraryExW");
			if (realLoadLibraryExW) {
				hook_install(&realLoadLibraryExW, (uintptr_t)fakeLoadLibraryExW);
			}
		}
	}

	void initialize(fn_attach attach, bool ap) {
		if (debuggerAttach) {
			if (!attachProcess && ap && hookDll) {
				attachProcess = ap;
				lua::hook(hookDll);
			}
			return;
		}
		debuggerAttach = attach;
		attachProcess  = ap;
		if (!findLuaDll()) {
			waitLuaDll();
		}
	}

	FARPROC luaapi(const char* name) {
#define FIND(api) \
		if (lua::real::##api && strcmp(name, #api) == 0) { \
			return (FARPROC)lua::real::##api; \
		}
		FIND(luaL_openlibs)
		FIND(lua_close)
		FIND(lua_settop)
		return 0;
#undef  FIND
	}
#else
	std::vector<RuntimeModule> GetProcessModuleMap(){
		static decltype(&GetProcessModuleMap) _GetProcessModuleMap = []()->decltype(&GetProcessModuleMap){
			Dl_info info;
			if (dladdr((void*)&DobbyHook, &info) == 0){
				return nullptr;
			}

			auto _GetProcessModuleMap = (decltype(&GetProcessModuleMap)) DobbySymbolResolver(info.dli_fname, "__ZN21ProcessRuntimeUtility19GetProcessModuleMapEv");
			if (!_GetProcessModuleMap)
				return nullptr;
			return _GetProcessModuleMap;
		}();

        auto modulemap = _GetProcessModuleMap();
#ifdef __linux__        
        // add exec module
        modulemap.push_back({{0},NULL});
#endif
		return modulemap;
	}
    void initialize(fn_attach attach, bool ap) {
		/*if (debuggerAttach) {
			if (!attachProcess && ap && hookDll) {
				attachProcess = ap;
				lua::hook(hookDll);
			}
			return;
		}*/
		debuggerAttach = attach;
		attachProcess  = ap;

        for (const auto &module : GetProcessModuleMap()) {  
			if (lua::hook(module))
				return;
        }
	}
#endif
}
