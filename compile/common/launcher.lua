local lm = require "luamake"

require "compile.common.frida"

lm:source_set "launcher_hook_luajit" {
    includes = {
        "3rd/lua/luajit/src",
        "3rd/frida_gum/gumpp",
        "src/launcher"
    },
    sources = "src/launcher/hook/luajit_listener.cpp",
}

lm:lua_source 'launcher_source' {
    deps = {
        "frida",
        "launcher_hook_luajit",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/frida_gum/gumpp",
        "3rd/lua/lua54",
        "3rd/json/single_include",
        "src/launcher",
    },
    sources = {
        "src/launcher/**/*.cpp",
        "!src/launcher/hook/luajit_listener.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
    windows = {
        defines = "_CRT_SECURE_NO_WARNINGS",
        links = {
            "ws2_32",
            "user32",
            "shell32",
            "ole32",
            "delayimp",
            "ntdll",
            "Version"
        },
        ldflags = {
            "/NODEFAULTLIB:LIBCMT"
        }
    },
    linux = {
        flags = "-fPIC",
    }
}
