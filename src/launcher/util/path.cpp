#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <util/path.h>

namespace luadebug {
    std::optional<fs::path> get_plugin_root() {
        auto dllpath = bee::path_helper::dll_path();
        if (!dllpath) {
            return std::nullopt;
        }
        return dllpath.value().parent_path().parent_path();
    }

    std::optional<fs::path> get_tmp_dir() {
        auto root = get_plugin_root();
        if (!root)
            return std::nullopt;
        return (*root) / "tmp";
    }

    std::optional<fs::path> get_runtime_dir() {
        auto root = get_plugin_root();
        if (!root)
            return std::nullopt;
        auto os =
#if defined(_WIN32)
            "windows";
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
            "darwin";
#else
            "linux";
#endif
        auto arch =
#if defined(_M_ARM64) || defined(__aarch64__)
            "arm64";
#elif defined(_M_IX86) || defined(__i386__)
            "ia32";
#elif defined(_M_X64) || defined(__x86_64__)
            "x64";
#else
#    error "Unknown architecture"
#endif
        auto platform = std::format("{}-{}", os, arch);
        return (*root) / "runtime" / platform;
    }

    std::optional<fs::path> get_lua_runtime_dir(autoattach::lua_version version) {
        auto runtime = get_runtime_dir();
        if (!runtime)
            return std::nullopt;
        return (*runtime) / autoattach::lua_version_to_string(version);
    }

    std::optional<fs::path> get_luadebug_path(autoattach::lua_version version) {
        auto runtime = get_lua_runtime_dir(version);
        if (!runtime)
            return std::nullopt;
#define LUADEBUG_FILE "luadebug"

#if defined(_WIN32)
#    define EXT ".dll"
#else
#    define EXT ".so"
#endif
        return (*runtime) / (LUADEBUG_FILE EXT);
    }

}
