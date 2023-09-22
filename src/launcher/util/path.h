#pragma once

#include <autoattach/lua_version.h>
#include <bee/nonstd/filesystem.h>

#include <optional>

namespace luadebug {
    std::optional<fs::path> get_plugin_root();
    std::optional<fs::path> get_tmp_dir();
    std::optional<fs::path> get_runtime_dir();
    std::optional<fs::path> get_lua_runtime_dir(autoattach::lua_version version);
    std::optional<fs::path> get_luadebug_path(autoattach::lua_version version);
}
