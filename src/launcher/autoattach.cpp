#include "autoattach.h"
#include <lua.hpp>
#include <mutex>
#include <stack>
#include <set>
#include <vector>
#include "../remotedebug/rdebug_delayload.h"
#ifdef _WIN32
#include <detours.h>
#include <intrin.h>
#include "fp_call.h"
#else
#include <dobby.h>
#include <dlfcn.h>
#include <string>
#include <string_view>
#include <memory>
#include <charconv>
#include "common.hpp"
#include "hook/hook_common.h"
#include "symbol_resolver/symbol_resolver.h"

class ProcessRuntimeUtility {
public:
    static const std::vector<RuntimeModule> &GetProcessModuleMap();
};
#endif

namespace autoattach {
	std::mutex lockLoadDll;
	fn_attach debuggerAttach;
	bool      attachProcess = false;
#ifdef _WIN32
	HMODULE   hookDll = NULL;
	std::set<std::wstring> loadedModules;
	std::set<lua_State*> hookLuaStates;
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
			uintptr_t luaL_openlibs = 0;
			uintptr_t lua_close = 0;
			uintptr_t lua_settop = 0;
		}
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

    static std::vector <RuntimeModule> GetProcessModuleMap() {
        return ProcessRuntimeUtility::GetProcessModuleMap();
    }

    bool iequals(const std::string_view &a, const std::string_view &b) {
        return std::equal(a.begin(), a.end(),
                          b.begin(), b.end(),
                          [](char a, char b) {
                              return tolower(a) == tolower(b);
                          });
    }

    lua_version get_lua_version_from_env() {
        const char *env_version = getenv("LUA_DEBUG_VERSION");
        if (env_version) {
            auto env = std::string_view(env_version);
            if (iequals(env, "jit"))
                return lua_version::luajit;
            if (iequals(env, "5.1"))
                return lua_version::lua51;
            if (iequals(env, "5.2"))
                return lua_version::lua52;
            if (iequals(env, "5.3"))
                return lua_version::lua53;
            if (iequals(env, "5.4") || iequals(env, "latest"))
                return lua_version::lua54;
        }
        return lua_version::unknown;
    }
	lua_version get_lua_version_from_ident(const char* lua_ident){
		auto id = std::string_view(lua_ident);
        using namespace std::string_view_literals;
        constexpr auto key = "$Lua"sv;
        if (id.substr(0, key.length()) == key) {
            if (id[key.length()] == ':') {
                //$Lua: Lua 5.1.*
                return lua_version::lua51;
            }
            constexpr auto key = "$LuaVersion: Lua 5."sv;
            id = id.substr(key.length());
            int patch;
            auto res = std::from_chars(id.data(), id.data() + 3, patch, 10);
            if (res.ec != std::errc()) {
                return lua_version::unknown;
            }
            return (lua_version) ((int) lua_version::luajit + patch);
        }
        return lua_version::unknown;
	}
    lua_version get_lua_version(const char *path) {
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta3")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta2")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta1")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_alpha")) {
            return lua_version::luajit;
        }
		auto p = DobbySymbolResolver(path, "lua_ident");;
        const char *lua_ident = (const char *) p;
        if (!lua_ident)
            return lua_version::unknown;
		return get_lua_version_from_ident(lua_ident);
    }

    bool is_lua_module(const RuntimeModule &module, bool signature) {
        if (signature) {
            //TODO:
        }
        return DobbySymbolResolver(module.path, "luaL_newstate");
    }

    static attach_args args;

    void attach_lua_vm(lua_State *L) {
        debuggerAttach(L, &args);
    }

    void initialize(fn_attach attach, bool ap) {
        debuggerAttach = attach;
        attachProcess = ap;

        auto modulemap = GetProcessModuleMap();
        bool signature = getenv("LUA_DEBUG_SIGNATURE") != nullptr;
        RuntimeModule rm = {};

        for (const auto &module: modulemap) {
            if (is_lua_module(module, signature)) {
                //check lua version
                rm = module;
                break;
            }
        }
        if (!rm.load_address) {
            LOG("can't find lua module");
            return;
        }
		auto symbol_resolver = symbol_resolver::symbol_resolver_factory_create(rm);
        if (!args.get_symbols(symbol_resolver.get())) {
            LOG("can't load args symbols");
            return;
        }

        auto luaversion = get_lua_version_from_env();
        if (luaversion == lua_version::unknown) {
            luaversion = get_lua_version(rm.path);
        }


        auto vmhook = create_vmhook(luaversion);
        if (!vmhook->get_symbols(symbol_resolver)) {
            LOG("get_symbols failed");
            return;
        }
        vmhook->hook();
    }
#endif
}
