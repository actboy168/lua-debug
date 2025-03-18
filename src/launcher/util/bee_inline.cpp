#if defined _WIN32
#    include <winsock2.h>
#endif

#include <3rd/fmt/format.cc>
#include <bee/lua/error.cpp>
#include <bee/net/endpoint.cpp>
#include <bee/net/socket.cpp>
#include <bee/sys/file_handle.cpp>

#if defined(_WIN32)
#    include <bee/net/uds_win.cpp>
#    include <bee/sys/file_handle_win.cpp>
#    include <bee/sys/path_win.cpp>
#    include <bee/win/wtf8.cpp>
#else
#    include <bee/sys/file_handle_posix.cpp>
#    include <bee/sys/path_posix.cpp>
#    if defined(__APPLE__)
#        include <bee/sys/file_handle_osx.cpp>
#        include <bee/sys/path_osx.cpp>
#    elif defined(__linux__)
#        include <bee/sys/file_handle_linux.cpp>
#        include <bee/sys/path_linux.cpp>
#    elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#        include <bee/sys/file_handle_bsd.cpp>
#        include <bee/sys/path_bsd.cpp>
#    endif
#endif
