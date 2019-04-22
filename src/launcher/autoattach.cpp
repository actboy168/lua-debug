#include "autoattach.h"
#include <lua.hpp>
#include <mutex>
#include <stack>
#include <set>
#include <vector>
#include <base/hook/fp_call.h>
#include <base/hook/inline.h>
#include <intrin.h>

namespace autoattach {
	std::mutex lockLoadDll;
	std::set<std::wstring> loadedModules;
	HMODULE luaDll = NULL;
	fn_attach debuggerAttach;
	fn_detach debuggerDetach;
	bool	  attachProcess = false;

	namespace lua {
		namespace real {
			uintptr_t lua_newstate = 0;
			uintptr_t lua_close = 0;
			uintptr_t lua_settop = 0;
		}
		namespace fake {
			static lua_State* __cdecl lua_newstate(void* f, void* ud) {
				lua_State* L = base::c_call<lua_State*>(real::lua_newstate, f, ud);
				debuggerAttach(L);
				return L;
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

			HOOK(lua_newstate);
			HOOK(lua_close);
			if (attachProcess) {
				HOOK(lua_settop);
			}

			std::stack<base::hook::hook_t> rollback;
			for (auto& task : tasks) {
				base::hook::hook_t h;
				if (!base::hook::install(&task.real, task.fake, &h)) {
					while (!rollback.empty()) {
						base::hook::uninstall(&rollback.top());
						rollback.pop();
					}
					return false;
				}
				rollback.push(h);
			}
			luaDll = m;
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
		if (hModuleKernel)
		{
			realLoadLibraryExW = (uintptr_t)GetProcAddress(hModuleKernel, "LoadLibraryExW");
			if (realLoadLibraryExW) {
				base::hook::install(&realLoadLibraryExW, (uintptr_t)fakeLoadLibraryExW);
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

	HMODULE luadll() {
		return luaDll;
	}
}
