#include <Windows.h>
#include <mutex>
#include <stack>
#include <set>
#include <base/hook/inline.h>
#include <base/hook/fp_call.h>
#include <base/path/self.h>
#include <base/util/format.h>
#include <debugger/debugger.h>
#include <debugger/io/namedpipe.h>

struct DebuggerConfig {
	static DebuggerConfig& Get() {
		static DebuggerConfig s_instance;
		return s_instance;
	}

	HMODULE luaDll = 0;
	fs::path binPath;
	std::wstring pipeName;

	DebuggerConfig() {
		binPath = base::path::self().parent_path();
	}

	bool initialize() {
		pipeName = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());
		return true;
	}
};

struct DebuggerWatcher {
	static DebuggerWatcher& Get() {
		static DebuggerWatcher s_instance;
		return s_instance;
	}

	std::unique_ptr<vscode::io::namedpipe> io;
	std::unique_ptr<vscode::debugger>      dbg;

	DebuggerWatcher() {
		auto& config = DebuggerConfig::Get();
		if (!GetModuleHandleW(L"debugger.dll")) {
			if (!LoadLibraryW((config.binPath / L"debugger.dll").c_str())) {
				return;
			}
		}
		debugger_set_luadll(config.luaDll, ::GetProcAddress);
		io.reset(new vscode::io::namedpipe());
		if (!io->open_server(config.pipeName)) {
			return;
		}
		io->kill_when_close();
		dbg.reset(new vscode::debugger(io.get()));
		dbg->wait_client();
		dbg->open_redirect(vscode::eRedirect::stdoutput);
		dbg->open_redirect(vscode::eRedirect::stderror);
	}
	void attach(lua_State* L) {
		if (dbg && L) dbg->attach_lua(L);
	}
	void detach(lua_State* L) {
		if (dbg && L) dbg->detach_lua(L, true);
	}
};

static HMODULE EnumerateModulesInProcess(HANDLE hProcess, HMODULE hModuleLast, PIMAGE_NT_HEADERS32 pNtHeader)
{
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

namespace lua {
	namespace real {
		uintptr_t lua_newstate = 0;
		uintptr_t luaL_newstate = 0;
		uintptr_t lua_close = 0;
	}
	namespace fake {
		static void* __cdecl lua_newstate(void* f, void* ud)
		{
			lua_State* L = base::c_call<lua_State*>(real::lua_newstate, f, ud);
			DebuggerWatcher::Get().attach(L);
			return L;
		}
		static void* __cdecl luaL_newstate()
		{
			lua_State* L = base::c_call<lua_State*>(real::luaL_newstate);
			DebuggerWatcher::Get().attach(L);
			return L;
		}
		void __cdecl lua_close(lua_State* L)
		{
			DebuggerWatcher::Get().detach(L);
			Sleep(1000);
			return base::c_call<void>(real::lua_close, L);
		}
	}

	bool hook(HMODULE m)
	{
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
		HOOK(luaL_newstate);
		HOOK(lua_close);

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
		DebuggerConfig::Get().luaDll = m;
		return true;
	}
}

std::mutex lockLoadDll;
std::set<std::wstring> loadedModules;

static bool TryHookLuaDll(HMODULE hModule)
{
	wchar_t moduleName[MAX_PATH];
	GetModuleFileNameW(hModule, moduleName, MAX_PATH);
	if (loadedModules.find(moduleName) != loadedModules.end()) {
		return false;
	}
	loadedModules.insert(moduleName);
	return lua::hook(hModule);
}

static bool FindLuaDll()
{
	std::unique_lock<std::mutex> lock(lockLoadDll);
	HANDLE hProcess = GetCurrentProcess();
	HMODULE hModule = NULL;
	for (;;) {
		IMAGE_NT_HEADERS32 inh;
		if ((hModule = EnumerateModulesInProcess(hProcess, hModule, &inh)) == NULL) {
			break;
		}
		if (TryHookLuaDll(hModule)) {
			return true;
		}
	}
	return false;
}

uintptr_t realLoadLibraryExW = 0;
HMODULE __stdcall fakeLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE hModule = base::std_call<HMODULE>(realLoadLibraryExW, lpLibFileName, hFile, dwFlags);
	std::unique_lock<std::mutex> lock(lockLoadDll);
	TryHookLuaDll(hModule);
	return hModule;
}

static void WaitLuaDll()
{
	HMODULE hModuleKernel = GetModuleHandleW(L"kernel32.dll");
	if (hModuleKernel)
	{
		realLoadLibraryExW = (uintptr_t)GetProcAddress(hModuleKernel, "LoadLibraryExW");
		if (realLoadLibraryExW) {
			base::hook::install(&realLoadLibraryExW, (uintptr_t)fakeLoadLibraryExW);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		::DisableThreadLibraryCalls(module);
		if (!DebuggerConfig::Get().initialize()) {
			MessageBoxW(0, L"debuggerInject initialize failed.", L"Error!", 0);
			return TRUE;
		}
		if (!FindLuaDll()) {
			WaitLuaDll();
		}
	}
	return TRUE;
}
