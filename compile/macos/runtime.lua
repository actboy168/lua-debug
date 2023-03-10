local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.frida"

lm:source_set "launcher_hook_luajit" {
    includes = {
        "3rd/lua/luajit/src",
        "3rd/frida_gum/gumpp",
        "src/launcher"
    },
    sources = "src/launcher/hook/luajit_listener.cpp",
}

lm:lua_library 'launcher' {
    export_luaopen = "off",
    deps = {
        "frida",
        "launcher_hook_luajit",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/frida_gum/gumpp",
        "3rd/lua/lua54",
        "src/launcher",
    },
    sources = {
        "src/launcher/**/*.cpp",
        "!src/launcher/hook/luajit_listener.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
}
