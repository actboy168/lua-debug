#if defined _WIN32
#    include <winsock2.h>
#endif

#define RLUA_REPLACE
#include "rlua.h"
#include "rdebug_cmodule.h"

#if !defined(luaL_newlibtable)
#    define luaL_newlibtable(L, l) \
        lua_createtable(L, 0, sizeof(l) / sizeof((l)[0]) - 1)
#endif

#if !defined(LUAI_UACINT)
#    if defined(_MSC_VER)
#        define LUAI_UACINT __int64
#    else
#        define LUAI_UACINT long long
#    endif
#endif

#if !defined(LUA_INTEGER_FMT)
#    if defined(_MSC_VER)
#        define LUA_INTEGER_FMT "%I64d"
#    else
#        define LUA_INTEGER_FMT "%lld"
#    endif
#endif

#if !defined(LUA_GCGEN)
#    define LUA_GCGEN 10
#endif

#include <binding/file.h>
#include <bee/error.cpp>
#include <bee/net/socket.cpp>
#include <bee/net/endpoint.cpp>
#include <bee/nonstd/3rd/format.cc>
#include <bee/utility/path_helper.cpp>
#include <bee/utility/file_handle.cpp>
#include <binding/lua_socket.cpp>
#include <binding/lua_thread.cpp>
#include <binding/lua_filesystem.cpp>

#if defined(_WIN32)
#    include <bee/platform/win/unicode_win.cpp>
#    include <bee/platform/win/unlink_win.cpp>
#    include <bee/utility/file_handle_win.cpp>
#    include <bee/thread/simplethread_win.cpp>
#    include <binding/lua_unicode.cpp>
#else
#    include <bee/thread/simplethread_posix.cpp>
#    include <bee/utility/file_handle_posix.cpp>
#    include <bee/subprocess/subprocess_posix.cpp>
#endif

#if defined(__APPLE__)
#    include <bee/utility/file_handle_osx.cpp>
#elif defined(__linux__)
#    include <bee/utility/file_handle_linux.cpp>
#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#    include <bee/utility/file_handle_bsd.cpp>
#endif
