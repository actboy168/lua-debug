#pragma once

#include <optional>
#include <string>

namespace remotedebug {
    std::optional<std::string> get_functioninfo(void* ptr);
}
