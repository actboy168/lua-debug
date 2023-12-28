#if defined _WIN32
#    include <winsock2.h>
#endif

#include <3rd/fmt/format.cc>
#include <bee/error.cpp>
#include <bee/net/endpoint.cpp>
#include <bee/net/socket.cpp>
#include <bee/thread/atomic_semaphore.cpp>
#include <bee/thread/setname.cpp>
#include <bee/thread/spinlock.cpp>
#include <bee/utility/file_handle.cpp>
#include <bee/utility/path_helper.cpp>

#if defined(_WIN32)
#    include <bee/platform/win/unicode_win.cpp>
#    include <bee/platform/win/unlink_win.cpp>
#    include <bee/thread/simplethread_win.cpp>
#    include <bee/utility/file_handle_win.cpp>
#else
#    include <bee/subprocess/subprocess_posix.cpp>
#    include <bee/thread/simplethread_posix.cpp>
#    include <bee/utility/file_handle_posix.cpp>
#endif

#if defined(__APPLE__)
#    include <bee/utility/file_handle_osx.cpp>
#elif defined(__linux__)
#    include <bee/utility/file_handle_linux.cpp>
#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#    include <bee/utility/file_handle_bsd.cpp>
#endif
