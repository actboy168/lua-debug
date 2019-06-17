#include "autoattach.h"
#include <lua.hpp>
#include <mutex>
#include <stack>
#include <set>
#include <vector>
#include <base/hook/fp_call.h>
#include <intrin.h>
#include <detours.h>
#include "../remotedebug/rdebug_delayload.h"

namespace autoattach {
	std::mutex lockLoadDll;
	std::set<std::wstring> loadedModules;
	fn_attach debuggerAttach;
	fn_detach debuggerDetach;
	bool	  attachProcess = false;

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

	namespace lua {
		namespace real {
			uintptr_t lua_newstate = 0;
			uintptr_t luaL_openlibs = 0;
			uintptr_t lua_close = 0;
			uintptr_t lua_settop = 0;
		}
		namespace fake {
			static lua_State* __cdecl lua_newstate(void* f, void* ud) {
				lua_State* L = base::c_call<lua_State*>(real::lua_newstate, f, ud);
				debuggerAttach(L);
				return L;
			}
			static void __cdecl luaL_openlibs(lua_State* L) {
				base::c_call<void>(real::luaL_openlibs, L);
				debuggerAttach(L);
			}
			static void __cdecl lua_settop(lua_State *L, int index) {
				debuggerAttach(L);
				return base::c_call<void>(real::lua_settop, L, index);
			}
			static void __cdecl lua_close(lua_State* L) {
				debuggerDetach(L);
				return base::c_call<void>(real::lua_close, L);
			}
		}

		bool hook(HMODULE m) {
			struct Hook {
				uintptr_t& real;
				uintptr_t fake;
			};
			std::vector<Hook> tasks;

#define HOOK(name) do {\
			real::##name = (uintptr_t)GetProcAddress(m, #name); \
			if (!real::##name) return false; \
			tasks.push_back({real::##name, (uintptr_t)fake::##name}); \
		} while (0)

			//HOOK(lua_newstate);
			HOOK(luaL_openlibs);
			HOOK(lua_close);
			if (attachProcess) {
				HOOK(lua_settop);
			}

			for (size_t i = 0; i < tasks.size(); ++i) {
				if (!hook_install(&tasks[i].real, tasks[i].fake)) {
					for (ptrdiff_t j = i - 1; j >= 0; ++j) {
						hook_uninstall(&tasks[j].real, tasks[j].fake);
					}
					return false;
				}
			}
			remotedebug::delayload::set_luadll(m);
			remotedebug::delayload::set_luaapi(luaapi);
			return true;
		}
	}

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
		wchar_t moduleName[MAX_PATH];
		GetModuleFileNameW(hModule, moduleName, MAX_PATH);
		if (loadedModules.find(moduleName) != loadedModules.end()) {
			return false;
		}
		loadedModules.insert(moduleName);
		return lua::hook(hModule);
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

	void initialize(fn_attach attach, fn_detach detach, bool ap) {
		debuggerAttach = attach;
		debuggerDetach = detach;
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
		//FIND(lua_newstate)
		FIND(luaL_openlibs)
		FIND(lua_close)
		FIND(lua_settop)
		return 0;
#undef  FIND
	}
}
