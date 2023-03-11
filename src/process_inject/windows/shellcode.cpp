#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
constexpr auto kernal32_dll_name = "Kernel32.dll";
constexpr auto LoadLibraryA_name = "LoadLibraryA";
constexpr auto GetLastError_name = "GetLastError";
struct rmain_arg {           // dealloc
    const char *name;       
    decltype(&GetModuleHandleA) GetModuleHandleA;
    decltype(&GetProcAddress) GetProcAddress;
	char entrypoint[256];
    const char key_kernal32_dll[sizeof(kernal32_dll_name)];
    const char key_LoadLibraryA[sizeof(LoadLibraryA_name)];
    const char key_GetLastError[sizeof(GetLastError_name)];
    FORCEINLINE void *get_func(HMODULE handle, const char *func_name) {
        return this->GetProcAddress(handle, func_name);
    }
};

__declspec(dllexport) DWORD WINAPI inject(_In_ LPVOID ptr) {
    if (!ptr) {
        return ERROR_INVALID_HANDLE;
    }
    rmain_arg& arg = *(rmain_arg *) ptr;
    HMODULE Kernel32_dll = arg.GetModuleHandleA(arg.key_kernal32_dll);
    auto _LoadLibraryA = (decltype(&LoadLibraryA)) arg.get_func(Kernel32_dll, arg.key_LoadLibraryA);
    HMODULE handler = _LoadLibraryA(arg.name);
    if (!handler) {
        auto _GetLastError = (decltype(&GetLastError)) arg.get_func(Kernel32_dll, arg.key_GetLastError);
        return _GetLastError();
    }
     void (*func)();
    func = (decltype(func)) arg.GetProcAddress(handler, arg.entrypoint);
    if (func)
        func();
    return 0;
}
}

