#pragma once

#include <string>

namespace luadebug::autoattach {
    bool wait_dll(bool (*loaded)(const std::string &));
}
