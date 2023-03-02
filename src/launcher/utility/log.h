#pragma once

namespace luadebug::log {
    void init(bool attach);
    void info(const char* msg);
    void fatal(const char* msg);
}
