#pragma once

#include <autoattach/lua_version.h>
#include <bee/nonstd/filesystem.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace luadebug::autoattach {
    struct session {
        work_mode mode;
        lua_version version = lua_version::unknown;
        std::string lua_module;
    };
    std::optional<session> init_session(work_mode mode);
}
