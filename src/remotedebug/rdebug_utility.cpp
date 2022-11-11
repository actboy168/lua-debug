#include "rlua.h"
#if defined(_WIN32)
#include <Windows.h>
#include <tlhelp32.h>
#endif
#include <signal.h>

namespace rdebug_utility {
#if defined(_WIN32)
    static bool closeWindow() {
        bool ok = false;
        HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (h != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(h, &te); ok; ok = Thread32Next(h, &te)) {
                if (te.th32OwnerProcessID == GetCurrentProcessId()) {
                    BOOL suc = PostThreadMessageW(te.th32ThreadID, WM_QUIT, 0, 0);
                    ok = ok || suc;
                }
            }
            CloseHandle(h);
        }
        return ok;
    }
    bool isConsoleExe(const wchar_t* exe) {
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

    static int closewindow(rlua_State* L) {
        bool ok = false;
#if defined(_WIN32)
        ok = closeWindow();
#endif
        rlua_pushboolean(L, ok);
        return 1;
    }

    static int closeprocess(rlua_State* L) {
        raise(SIGINT);
        return 0;
    }
}

RLUA_FUNC
int luaopen_remotedebug_utility(rlua_State* L) {
    rlua_newtable(L);
    rluaL_Reg lib[] = {
        {"closewindow", rdebug_utility::closewindow},
        {"closeprocess", rdebug_utility::closeprocess},
        {NULL, NULL}};
    rluaL_setfuncs(L, lib, 0);
    return 1;
}
