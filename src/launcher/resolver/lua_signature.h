#pragma once
#include <resolver/lua_delayload.h>

#include <string>
#include <string_view>

namespace luadebug::autoattach {
    struct signature {
        int32_t start_offset = 0;
        int32_t end_offset   = 0;
        std::string pattern;
        int32_t pattern_offset = 0;
        uint8_t hit_offset     = 0;
        intptr_t find(const char* module_name) const;
    };

    struct signature_resolver : lua::resolver {
        ~signature_resolver() = default;
        intptr_t find(std::string_view name) const override;
        std::string module_name;
        std::unique_ptr<lua::resolver> resolver;
        std::unique_ptr<lua::resolver> reserve_resolver;
    };
}