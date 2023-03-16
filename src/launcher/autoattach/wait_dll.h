#pragma once

#include <string>

namespace luadebug::autoattach {
    bool wait_dll(bool (*loaded)(std::string const&));
}
