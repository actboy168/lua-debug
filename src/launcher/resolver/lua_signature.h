#pragma once
#include <string>

namespace luadebug::autoattach {
    struct signture {
        std::string name;
        int32_t start_offset = 0;
        int32_t end_offset = 0;
        std::string pattern;
        int32_t pattern_offset = 0;
        uint8_t hit_offset = 0;
    };
}