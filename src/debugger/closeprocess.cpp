#include <signal.h>
#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace vscode {
#if defined(_WIN32)
	static int getThreadId(int pid)
	{
		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (h != INVALID_HANDLE_VALUE) {
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			for (BOOL ok = Thread32First(h, &te); ok; ok = Thread32Next(h, &te)) {
				if (te.th32OwnerProcessID == pid) {
					CloseHandle(h);
					return te.th32ThreadID;
				}
			}
		}
		CloseHandle(h);
		return 0;
	}

	static void closeWindow() {
		int tid = getThreadId(GetCurrentProcessId());
		PostThreadMessageW(tid, WM_CLOSE, 0, 0);
		PostThreadMessageW(tid, WM_QUIT, 0, 0);
	}

	bool isConsoleExe(const wchar_t* exe)
	{
		HANDLE hExe = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hExe == 0) {
			return false;
		}
		DWORD read;
		char data[sizeof IMAGE_NT_HEADERS + sizeof IMAGE_DOS_HEADER];
		SetFilePointer(hExe, 0, NULL, FILE_BEGIN);
		if (!ReadFile(hExe, data, sizeof IMAGE_DOS_HEADER, &read, NULL)) {
			CloseHandle(hExe);
			return false;
		}
		SetFilePointer(hExe, ((PIMAGE_DOS_HEADER)data)->e_lfanew, NULL, FILE_BEGIN);
		if (!ReadFile(hExe, data, sizeof IMAGE_NT_HEADERS, &read, NULL)) {
			CloseHandle(hExe);
			return false;
		}
		CloseHandle(hExe);
		return ((PIMAGE_NT_HEADERS)data)->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI;
	}

	static bool isConsoleProcess() {
		wchar_t exe[MAX_PATH] = { 0 };
		GetModuleFileNameW(NULL, exe, MAX_PATH);
		return isConsoleExe(exe);
	}
#endif

	void closeprocess() {
#if defined(_WIN32)
		closeWindow();
		if (isConsoleProcess()) {
			raise(SIGINT);
		}
#else
		raise(SIGINT);
#endif
	}
}
