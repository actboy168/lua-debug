#include <Windows.h>
#include <base/hook/inline.h>
#include <base/hook/fp_call.h>
#include <base/win/process.h>
#include <mutex>
#include "utility.h"
#include "inject.h"

#pragma data_seg("Shared")
bool                         shared_enable = false;
#pragma data_seg()
#pragma comment(linker, "/section:Shared,rws")

HMODULE luadll = 0;

void initialize_debugger(void* L)
{
	if (GetModuleHandleW(L"debugger.dll")) {
		return;
	}
	fs::path debugger = get_self_path().remove_filename() / L"debugger.dll";
	HMODULE dll = LoadLibraryW(debugger.c_str());
	if (!dll) {
		return;
	}
	uintptr_t set_luadll   = (uintptr_t)GetProcAddress(dll, "set_luadll");
	uintptr_t start_server = (uintptr_t)GetProcAddress(dll, "start_server");
	uintptr_t attach_lua   = (uintptr_t)GetProcAddress(dll, "attach_lua");
	if (!set_luadll || !start_server || !attach_lua) {
		return;
	}
	base::c_call<void>(set_luadll, luadll);
	uint16_t port = base::c_call<uint16_t>(start_server, "127.0.0.1", 0, true, false);
	base::c_call<void>(attach_lua, L, true);
}

void uninitialize_debugger(void* L)
{
	HMODULE dll = GetModuleHandleW(L"debugger.dll");
	if (!dll) {
		return;
	}
	uintptr_t detach_lua = (uintptr_t)GetProcAddress(dll, "detach_lua");
	if (!detach_lua) {
		return;
	}
	base::c_call<void>(detach_lua, L);
	Sleep(1000);
}

struct lua_newstate
	: public hook_helper<lua_newstate>
{
	static void* __cdecl fake(void* f, void* ud)
	{
		void* L = base::c_call<void*>(real, f, ud);
		if (L) initialize_debugger(L);
		return L;
	}
};

struct luaL_newstate
	: public hook_helper<luaL_newstate>
{
	static void* __cdecl fake()
	{
		void* L = base::c_call<void*>(real);

		if (L) initialize_debugger(L);
		return L;
	}
};

struct lua_close
	: public hook_helper<lua_close>
{
	static void __cdecl fake(void* L)
	{
		uninitialize_debugger(L);
		return base::c_call<void>(real, L);
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

static bool IsLuaDll(HMODULE hModule)
{
	if (GetProcAddress(hModule, "lua_close") && (GetProcAddress(hModule, "lua_newstate") || GetProcAddress(hModule, "luaL_newstate"))) {
		return true;
	}
	return false;
}

static bool HookLuaDll(HMODULE hModule)
{
	if (!luadll) {
		luadll = hModule;
		lua_newstate::init(hModule, "lua_newstate");
		luaL_newstate::init(hModule, "luaL_newstate");
		lua_close::init(hModule, "lua_close");
	}
	return true;
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
	if (IsLuaDll(hModule)) {
		HookLuaDll(hModule);
		return true;
	}
	return false;
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
		if (shared_enable) {
			if (FindLuaDll()) {
				WaitLuaDll();
			}
		}
	}
	return TRUE;
}

void inject_start(base::win::process& p)
{
	shared_enable = true;
	p.inject(get_self_path());
}
