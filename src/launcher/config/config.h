#pragma once
#include <autoattach/lua_version.h>
#include <bee/nonstd/filesystem.h>
#include <resolver/lua_signature.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace luadebug::config {
    using lua_version = autoattach::lua_version;
    using signature   = autoattach::signature;
    std::optional<fs::path> get_plugin_root();
    std::optional<fs::path> get_tmp_dir();
    std::optional<fs::path> get_runtime_dir();
    std::optional<fs::path> get_lua_runtime_dir(lua_version version);
    std::optional<fs::path> get_luadebug_path(lua_version version);
    struct Config {
        lua_version version = lua_version::unknown;
        std::string lua_module;
        std::map<std::string, signature> signatures;
        bool is_signature_mode() const;
    };
    std::optional<Config> init_from_file();
}  // namespace luadebug::config