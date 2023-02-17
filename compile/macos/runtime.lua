local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.dobby"

lm:source_set "std_fmt" {
    includes = "3rd/bee.lua/bee/nonstd",
    sources = "3rd/bee.lua/bee/nonstd/fmt/*.cc"
}

lm:source_set "launcher_threads" {
    windows = {
        sources = {
            "src/launcher/thread/threads_windows.cpp",
        }
    },
    linux = {
        sources = {
            "src/launcher/thread/threads_linux.cpp",
        }
    },
    macos = {
        sources = {
            "src/launcher/thread/threads_macos.cpp",
        }
    }
}

lm:lua_library('liblauncher') {
    export_luaopen = "off",
    deps = {
        "dobby",
        "launcher_threads",
        "std_fmt",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/dobby/include",
        "3rd/lua/luajit/src",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "src/launcher/*.cpp",
        "src/launcher/hook/*.cpp",
        "src/launcher/symbol_resolver/*.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
}
