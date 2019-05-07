local lm = require "luamake"

lm.arch = ARGUMENTS.arch or 'x64'
lm.bindir = ("build/%s/bin/%s"):format(lm.plat, lm.arch)
lm.objdir = ("build/%s/obj/%s"):format(lm.plat, lm.arch)

lm:import '3rd/bee.lua/make.lua'

lm:source_set 'detours' {
    rootdir = "3rd/detours/src",
    permissive = true,
    sources = {
        "*.cpp",
        "!uimports.cpp"
    }
}

lm:shared_library 'launcher' {
    deps = {
        "lua54",
        "detours",
    },
    includes = {
        "src",
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua/src",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/error/category_win.cpp",
        "3rd/bee.lua/bee/utility/unicode_win.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_helper.cpp",
        "src/base/hook/inline.cpp",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/*.cpp",
    },
    defines = {
        "BEE_INLINE",
        "_CRT_SECURE_NO_WARNINGS",
    },
    links = {
        "ws2_32",
        "user32",
        "delayimp",
    },
    ldflags = '/DELAYLOAD:lua54.dll',
}

lm:default {
    "launcher",
}
