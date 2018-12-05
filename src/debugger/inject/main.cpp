#include <Windows.h>
#include <bee/utility/path_helper.h>
#include <bee/utility/format.h>
#include <base/win/process_switch.h>
#include <debugger/debugger.h>
#include <debugger/io/namedpipe.h>
#include <debugger/inject/autoattach.h>

static vscode::debugger* dbg = nullptr;
static std::filesystem::path binPath;
static std::wstring pipeName;
static bool isAttachProcess = false;

static bool initialize() {
	binPath = bee::path_helper::dll_path().value().parent_path();
	int pid = (int)GetCurrentProcessId();
	pipeName = bee::format(L"vscode-lua-debug-%d", pid);
	isAttachProcess = base::win::process_switch::has(pid, L"attachprocess");
	return true;
}

static bool createDebugger() {
	if (!GetModuleHandleW(L"debugger.dll")) {
		if (!LoadLibraryW((binPath / L"debugger.dll").c_str())) {
			return false;
		}
	}
	debugger_set_luadll(autoattach::luadll(), ::GetProcAddress);
	debugger_create(pipeName.c_str());
	dbg = debugger_get();
	dbg->wait_client();
	dbg->open_redirect(vscode::eRedirect::stdoutput);
	dbg->open_redirect(vscode::eRedirect::stderror);
	return true;
}

void debuggerAttach(lua_State* L) {
	if (!L) return;
	if (!dbg) {
		if (!createDebugger()) {
			return;
		}
	}
	if (dbg->attach_lua(L)) {
		dbg->init_internal_module(L);
	}
}

void debuggerDetach(lua_State* L) {
	if (!L) return;
	if (dbg) dbg->detach_lua(L, true);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		::DisableThreadLibraryCalls(module);
		if (!initialize()) {
			MessageBoxW(0, L"debuggerInject initialize failed.", L"Error!", 0);
			return TRUE;
		}
		autoattach::initialize(debuggerAttach, debuggerDetach, isAttachProcess);
	}
	return TRUE;
}
