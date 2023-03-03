local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.frida"

lm:source_set "std_format" {
    includes = "3rd/bee.lua/bee/nonstd/3rd/fmt",
    sources = {
        "3rd/bee.lua/bee/nonstd/3rd/format.cc",
        "3rd/bee.lua/bee/nonstd/3rd/os.cc",
    }
}

lm:source_set "launcher_hook_luajit" {
    includes = {
        "3rd/lua/luajit/src",
        "3rd/frida_gum/gumpp",
        "src/launcher"
    },
    sources = "src/launcher/hook/luajit_listener.cpp",
}

lm:lua_library('liblauncher') {
    export_luaopen = "off",
    deps = {
        "frida",
        "std_format",
        "launcher_hook_luajit",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/frida_gum/gumpp",
        "3rd/lua/lua54",
        "src/launcher",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/net/endpoint.cpp",
        "3rd/bee.lua/bee/net/socket.cpp",
        "src/launcher/**/*.cpp",
        "!src/launcher/hook/luajit_listener.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
}
