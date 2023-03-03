#pragma once


#include <bee/nonstd/format.h>

#if !defined(__cpp_lib_format)
namespace std {
    using ::fmt::format_string;
}
#endif

namespace luadebug::log {
    void init(bool attach);
    void notify_frontend(const std::string& msg);

    template <typename... T>
    inline void error(std::format_string<T...> fmt, T&&... args) {
        std::print(stderr, "[lua-debug][launcher]{}\n", std::format(fmt, std::forward<T>(args)...));
    }

#ifdef NDEBUG
    template <typename... T>
    inline void info(std::format_string<T...> fmt, T&&... args)
    {}
#else
    template <typename... T>
    using info = error<T...>;
#endif

    template <typename... T>
    inline void fatal(std::format_string<T...> fmt, T&&... args) {
        auto msg = std::format(fmt, std::forward<T>(args)...);
#ifndef NDEBUG
        error("{}", msg);
#endif
        notify_frontend(msg);
    }
}
