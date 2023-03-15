#pragma once

#include <optional>
#include <string>

namespace luadebug {
    std::optional<std::string> get_functioninfo(void* ptr);
}
