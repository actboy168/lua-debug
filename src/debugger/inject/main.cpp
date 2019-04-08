#include <Windows.h>
#include <bee/utility/path_helper.h>
#include <bee/utility/format.h>
#include <debugger/debugger.h>
#include <debugger/inject/autoattach.h>
#include <debugger/client/get_unix_path.h>

static vscode::debugger* dbg = nullptr;
static std::filesystem::path binPath;
static std::string pipeName;

static bool createDebugger() {
	if (!GetModuleHandleW(L"debugger.dll")) {
		if (!LoadLibraryW((binPath / L"debugger.dll").c_str())) {
			return false;
		}
	}
	debugger_set_luadll(autoattach::luadll(), ::GetProcAddress);
    if (!debugger_create(pipeName.c_str())) {
        return false;
    }
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
    dbg->attach_lua(L);
}

void debuggerDetach(lua_State* L) {
	if (!L) return;
	if (dbg) dbg->detach_lua(L, true);
}

static void initialize(bool attachprocess) {
    binPath = bee::path_helper::dll_path().value().parent_path();
    pipeName = get_unix_path((int)GetCurrentProcessId());
    autoattach::initialize(debuggerAttach, debuggerDetach, attachprocess);
}

extern "C" __declspec(dllexport)
void __cdecl launch() {
    initialize(false);
}

extern "C" __declspec(dllexport)
void __cdecl attach() {
    initialize(true);
}
