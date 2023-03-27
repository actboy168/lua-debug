#pragma once

#include <config/config.h>

#include <functional>
#include <string>

namespace luadebug::autoattach {
    using WaitDllCallBack_t = std::function<bool(const std::string&)>;
    bool wait_dll(WaitDllCallBack_t loaded);
}
