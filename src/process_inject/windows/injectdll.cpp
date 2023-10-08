#if defined(_M_X64)
#    error unsupport x86_64
#endif

#include "injectdll.h"

#include <Windows.h>
#include <stdint.h>
#include <tlhelp32.h>
#include <wow64ext.h>

static bool is_process64(HANDLE hProcess) {
    BOOL wow64 = FALSE;
    if (!IsWow64Process(hProcess, &wow64)) {
        return false;
    }
    return !wow64;
}

static uint64_t wow64_module(const wchar_t* name) {
    return GetModuleHandle64(name);
}

static uint64_t wow64_import(uint64_t module, const char* name) {
    return GetProcAddress64(module, name);
}

template <class... Args>
static uint64_t wow64_call(uint64_t func, Args... args) {
    return X64Call(func, sizeof...(args), (uint64_t)args...);
}

static uint64_t wow64_alloc_memory(uint64_t navm, HANDLE hProcess, SIZE_T dwSize, DWORD flProtect) {
    uint64_t tmpAddr = 0;
    uint64_t tmpSize = dwSize;
    uint64_t ret     = wow64_call(navm, hProcess, &tmpAddr, 0, &tmpSize, MEM_COMMIT, flProtect);
    if (0 != ret) {
        return 0;
    }
    return tmpAddr;
}

static bool wow64_write_memory(uint64_t nwvm, HANDLE hProcess, uint64_t lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize) {
    uint64_t written = 0;
    uint64_t res     = wow64_call(nwvm, hProcess, lpBaseAddress, lpBuffer, nSize, &written);
    if (0 != res || nSize != written) {
        return false;
    }
    return true;
}

static bool injectdll_x64(HANDLE process_handle, HANDLE thread_handle, const std::wstring& dll, const std::string_view& entry) {
    static unsigned char sc[] = {
        0x9C,                                                        // pushfq
        0x0F, 0xA8,                                                  // push gs
        0x0F, 0xA0,                                                  // push fs
        0x50,                                                        // push rax
        0x51,                                                        // push rcx
        0x52,                                                        // push rdx
        0x53,                                                        // push rbx
        0x55,                                                        // push rbp
        0x56,                                                        // push rsi
        0x57,                                                        // push rdi
        0x41, 0x50,                                                  // push r8
        0x41, 0x51,                                                  // push r9
        0x41, 0x52,                                                  // push r10
        0x41, 0x53,                                                  // push r11
        0x41, 0x54,                                                  // push r12
        0x41, 0x55,                                                  // push r13
        0x41, 0x56,                                                  // push r14
        0x41, 0x57,                                                  // push r15
        0x48, 0x83, 0xEC, 0x00,                                      // sub rsp, 0  // rsp
        0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  r9, 0  // DllHandle
        0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  r8, 0  // DllPath
        0x48, 0x31, 0xD2,                                            // xor  rdx,rdx
        0x48, 0x31, 0xC9,                                            // xor  rcx,rcx
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  rax,0  // LdrLoadDll
        0xFF, 0xD0,                                                  // call rax
        0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  r9, 0  // EntryFunc
        0x4D, 0x31, 0xC0,                                            // xor  r8, r8
        0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  rdx,0  // EntryName
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  rcx,0  // DllHandle
        0x48, 0x8B, 0x09,                                            // mov  rcx,[rcx]
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  rax,0  // LdrGetProcedureAddress
        0xFF, 0xD0,                                                  // call rax
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov  rax,0  // EntryFunc
        0x48, 0x8B, 0x00,                                            // mov  rax,[rax]
        0xFF, 0xD0,                                                  // call rax
        0x48, 0x83, 0xC4, 0x00,                                      // add rsp, 0  // rsp
        0x41, 0x5F,                                                  // pop r15
        0x41, 0x5E,                                                  // pop r14
        0x41, 0x5D,                                                  // pop r13
        0x41, 0x5C,                                                  // pop r12
        0x41, 0x5B,                                                  // pop r11
        0x41, 0x5A,                                                  // pop r10
        0x41, 0x59,                                                  // pop r9
        0x41, 0x58,                                                  // pop r8
        0x5F,                                                        // pop rdi
        0x5E,                                                        // pop rsi
        0x5D,                                                        // pop rbp
        0x5B,                                                        // pop rbx
        0x5A,                                                        // pop rdx
        0x59,                                                        // pop rcx
        0x58,                                                        // pop rax
        0x0F, 0xA1,                                                  // pop fs
        0x0F, 0xA9,                                                  // pop gs
        0x9D,                                                        // popfq
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,                          // jmp offset
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00               // rip
    };
    uint64_t ntdll = wow64_module(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }
    uint64_t pfLdrLoadDll             = wow64_import(ntdll, "LdrLoadDll");
    uint64_t pfLdrGetProcedureAddress = wow64_import(ntdll, "LdrGetProcedureAddress");
    if (!pfLdrLoadDll || !pfLdrGetProcedureAddress) {
        return false;
    }
    uint64_t pfNtGetContextThread      = wow64_import(ntdll, "NtGetContextThread");
    uint64_t pfNtSetContextThread      = wow64_import(ntdll, "NtSetContextThread");
    uint64_t pfNtAllocateVirtualMemory = wow64_import(ntdll, "NtAllocateVirtualMemory");
    uint64_t pfNtWriteVirtualMemory    = wow64_import(ntdll, "NtWriteVirtualMemory");
    if (!pfNtGetContextThread || !pfNtSetContextThread || !pfNtAllocateVirtualMemory || !pfNtWriteVirtualMemory) {
        return false;
    }
    struct UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        uint64_t Buffer;
    };
    SIZE_T mem1size = sizeof(uint64_t) + sizeof(UNICODE_STRING) + (dll.size() + 1) * sizeof(wchar_t);
    uint64_t mem1   = wow64_alloc_memory(pfNtAllocateVirtualMemory, process_handle, mem1size, PAGE_READWRITE);
    if (!mem1) {
        return false;
    }
    SIZE_T mem2size = sizeof(uint64_t) + sizeof(UNICODE_STRING) + (entry.size() + 1) * sizeof(char);
    uint64_t mem2   = wow64_alloc_memory(pfNtAllocateVirtualMemory, process_handle, mem2size, PAGE_READWRITE);
    if (!mem2) {
        return false;
    }
    uint64_t shellcode = wow64_alloc_memory(pfNtAllocateVirtualMemory, process_handle, sizeof(sc), PAGE_EXECUTE_READWRITE);
    if (!shellcode) {
        return false;
    }
    UNICODE_STRING us1;
    us1.Length        = (USHORT)(dll.size() * sizeof(wchar_t));
    us1.MaximumLength = us1.Length + sizeof(wchar_t);
    us1.Buffer        = mem1 + sizeof(UNICODE_STRING);
    if (!wow64_write_memory(pfNtWriteVirtualMemory, process_handle, mem1, &us1, sizeof(UNICODE_STRING))) {
        return false;
    }
    if (!wow64_write_memory(pfNtWriteVirtualMemory, process_handle, us1.Buffer, (void*)dll.data(), us1.MaximumLength)) {
        return false;
    }
    UNICODE_STRING us2;
    us2.Length        = (USHORT)(dll.size() * sizeof(wchar_t));
    us2.MaximumLength = us2.Length + sizeof(wchar_t);
    us2.Buffer        = mem2 + sizeof(UNICODE_STRING);
    if (!wow64_write_memory(pfNtWriteVirtualMemory, process_handle, mem2, &us2, sizeof(UNICODE_STRING))) {
        return false;
    }
    if (!wow64_write_memory(pfNtWriteVirtualMemory, process_handle, us2.Buffer, (void*)entry.data(), us2.MaximumLength)) {
        return false;
    }
    _CONTEXT64 ctx   = { 0 };
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (wow64_call(pfNtGetContextThread, thread_handle, &ctx)) {
        return false;
    }
    uint64_t dllhandle = us1.Buffer + us1.MaximumLength;
    uint64_t entryfunc = us2.Buffer + us2.MaximumLength;
    memcpy(sc + 34, &dllhandle, sizeof(dllhandle));
    memcpy(sc + 44, &mem1, sizeof(mem1));
    memcpy(sc + 60, &pfLdrLoadDll, sizeof(pfLdrLoadDll));
    memcpy(sc + 72, &entryfunc, sizeof(entryfunc));
    memcpy(sc + 85, &mem2, sizeof(mem2));
    memcpy(sc + 95, &dllhandle, sizeof(dllhandle));
    memcpy(sc + 108, &pfLdrGetProcedureAddress, sizeof(pfLdrGetProcedureAddress));
    memcpy(sc + 120, &entryfunc, sizeof(entryfunc));
    memcpy(sc + 171, &ctx.Rip, sizeof(ctx.Rip));
    {
        uint8_t v = 0x40 - (ctx.Rsp & 0x0F);
        memcpy(sc + 31, &v, 1);
        memcpy(sc + 136, &v, 1);
    }
    if (!wow64_write_memory(pfNtWriteVirtualMemory, process_handle, shellcode, &sc, sizeof(sc))) {
        return false;
    }
    ctx.ContextFlags = CONTEXT_CONTROL;
    ctx.Rip          = shellcode;
    if (wow64_call(pfNtSetContextThread, thread_handle, &ctx)) {
        return false;
    }
    return true;
}

static bool injectdll_x86(HANDLE process_handle, HANDLE thread_handle, const std::wstring& dll, const std::string_view& entry) {
    static unsigned char sc[] = {
        0x68, 0x00, 0x00, 0x00, 0x00,  // push eip
        0x9C,                          // pushfd
        0x60,                          // pushad
        0x68, 0x00, 0x00, 0x00, 0x00,  // push DllPath
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, LoadLibraryW
        0xFF, 0xD0,                    // call eax
        0x68, 0x00, 0x00, 0x00, 0x00,  // push EntryName
        0x50,                          // push eax
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, GetProcAddress
        0xFF, 0xD0,                    // call eax
        0xFF, 0xD0,                    // call eax
        0x61,                          // popad
        0x9D,                          // popfd
        0xC3                           // ret
    };
    DWORD pfLoadLibrary = (DWORD)::GetProcAddress(::GetModuleHandleW(L"Kernel32"), "LoadLibraryW");
    if (!pfLoadLibrary) {
        return false;
    }
    DWORD pfGetProcAddress = (DWORD)::GetProcAddress(::GetModuleHandleW(L"Kernel32"), "GetProcAddress");
    if (!pfGetProcAddress) {
        return false;
    }
    SIZE_T mem1size = (dll.size() + 1) * sizeof(wchar_t);
    LPVOID mem1     = VirtualAllocEx(process_handle, NULL, mem1size, MEM_COMMIT, PAGE_READWRITE);
    if (!mem1) {
        return false;
    }
    SIZE_T mem2size = (entry.size() + 1) * sizeof(char);
    LPVOID mem2     = VirtualAllocEx(process_handle, NULL, mem2size, MEM_COMMIT, PAGE_READWRITE);
    if (!mem2) {
        return false;
    }
    LPVOID shellcode = VirtualAllocEx(process_handle, NULL, sizeof(sc), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!shellcode) {
        return false;
    }
    SIZE_T written = 0;
    BOOL ok        = FALSE;
    ok             = WriteProcessMemory(process_handle, mem1, dll.data(), mem1size, &written);
    if (!ok || written != mem1size) {
        return false;
    }
    ok = WriteProcessMemory(process_handle, mem2, entry.data(), mem2size, &written);
    if (!ok || written != mem2size) {
        return false;
    }
    CONTEXT ctx      = { 0 };
    ctx.ContextFlags = CONTEXT_FULL;
    if (!::GetThreadContext(thread_handle, &ctx)) {
        return false;
    }
    memcpy(sc + 1, &ctx.Eip, sizeof(ctx.Eip));
    memcpy(sc + 8, &mem1, sizeof(mem1));
    memcpy(sc + 13, &pfLoadLibrary, sizeof(pfLoadLibrary));
    memcpy(sc + 20, &mem2, sizeof(mem2));
    memcpy(sc + 26, &pfGetProcAddress, sizeof(pfGetProcAddress));
    ok = WriteProcessMemory(process_handle, shellcode, &sc, sizeof(sc), &written);
    if (!ok || written != sizeof(sc)) {
        return false;
    }
    ctx.ContextFlags = CONTEXT_CONTROL;
    ctx.Eip          = (DWORD)shellcode;
    if (!::SetThreadContext(thread_handle, &ctx)) {
        return false;
    }
    return true;
}

bool injectdll(HANDLE process_handle, HANDLE thread_handle, const std::wstring& x86dll, const std::wstring& x64dll, const std::string_view& entry) {
    if (is_process64(process_handle)) {
        return !x64dll.empty() && injectdll_x64(process_handle, thread_handle, x64dll, entry);
    }
    else {
        return !x86dll.empty() && injectdll_x86(process_handle, thread_handle, x86dll, entry);
    }
}

static bool setdebugprivilege() {
    TOKEN_PRIVILEGES tp = { 0 };
    HANDLE hToken       = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);
    return true;
}

static DWORD getthreadid(DWORD pid) {
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

static void closeprocess(PROCESS_INFORMATION& pi) {
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);
    pi = { 0 };
}

static bool openprocess(DWORD pid, DWORD process_access, DWORD thread_access, PROCESS_INFORMATION& pi) {
    closeprocess(pi);
    static bool ok = setdebugprivilege();
    if (!ok) {
        return false;
    }
    pi.dwProcessId = pid;
    pi.hProcess    = OpenProcess(process_access, FALSE, pi.dwProcessId);
    if (!pi.hProcess) {
        closeprocess(pi);
        return false;
    }
    pi.dwThreadId = getthreadid(pi.dwProcessId);
    if (!pi.dwThreadId) {
        closeprocess(pi);
        return false;
    }
    pi.hThread = OpenThread(thread_access, FALSE, pi.dwThreadId);
    if (!pi.hThread) {
        closeprocess(pi);
        return false;
    }
    return true;
}

bool injectdll(DWORD pid, const std::wstring& x86dll, const std::wstring& x64dll, const std::string_view& entry) {
    PROCESS_INFORMATION pi = { 0 };
    if (!openprocess(pid, PROCESS_ALL_ACCESS, THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, pi)) {
        return false;
    }
    if (-1 == SuspendThread(pi.hThread)) {
        return false;
    }
    bool ok = injectdll(pi.hProcess, pi.hThread, x86dll, x64dll, entry);
    ResumeThread(pi.hThread);
    closeprocess(pi);
    return ok;
}
