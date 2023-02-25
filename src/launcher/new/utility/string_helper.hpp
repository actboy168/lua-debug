#pragma once

#include <vector>
#include <string_view>

namespace strings {
    template<typename T>
    std::vector <std::string_view> spilt_string(const std::string_view &str, const T delim) {
        std::vector <std::string_view> elems;
        auto lastPos = str.find_first_not_of(delim, 0);
        auto pos = str.find_first_of(delim, lastPos);
        while (pos != std::string_view::npos || lastPos != std::string_view::npos) {
            elems.push_back(str.substr(lastPos, pos - lastPos));
            lastPos = str.find_first_not_of(delim, pos);
            pos = str.find_first_of(delim, lastPos);
        }
        return elems;
    }
}