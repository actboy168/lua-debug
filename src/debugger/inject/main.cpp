#include <Windows.h>
#include <base/path/self.h>
#include <base/util/format.h>
#include <base/win/process_switch.h>
#include <debugger/debugger.h>
#include <debugger/io/namedpipe.h>
#include <debugger/inject/autoattach.h>

static std::unique_ptr<vscode::io::namedpipe> io;
static std::unique_ptr<vscode::debugger> dbg;
static fs::path binPath;
static std::wstring pipeName;
static bool isAttachProcess = false;

static bool initialize() {
	binPath = base::path::self().parent_path();
	int pid = (int)GetCurrentProcessId();
	pipeName = base::format(L"vscode-lua-debug-%d", pid);
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
	io.reset(new vscode::io::namedpipe());
	if (!io->open_server(pipeName)) {
		return false;
	}
	dbg.reset(new vscode::debugger(io.get()));
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
