#pragma once

#if defined(_WIN32)

#    include <Windows.h>
#    include <intrin.h>

namespace luadebug::win32 {
    HMODULE get_luadll();
    void set_luadll(HMODULE handle);
    void set_luaapi(void* fn);
    bool caller_is_luadll(void* callerAddress);
    bool find_luadll();
    void putenv(const char* envstr);
    void setflag_debugself(bool flag);
}

#endif
