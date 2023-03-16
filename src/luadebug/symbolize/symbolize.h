#pragma once

#include <optional>
#include <string>

namespace luadebug {
    std::optional<std::string> symbolize(const void* ptr);
}
