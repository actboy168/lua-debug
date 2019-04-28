local lm = require "luamake"

lm.arch = ARGUMENTS.arch or 'x64'
lm.bindir = ("build/%s/bin/%s"):format(lm.plat, lm.arch)
lm.objdir = ("build/%s/obj/%s"):format(lm.plat, lm.arch)

lm:shared_library 'lua54' {
    sources = {
        "3rd/bee.lua/3rd/lua/src/*.c",
        "!3rd/bee.lua/3rd/lua/src/lua.c",
        "!3rd/bee.lua/3rd/lua/src/luac.c",
        "3rd/bee.lua/3rd/lua/utf8/utf8_crt.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
        "LUAI_MAXCSTACK=1000"
    }
}

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
        "include",
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua/src",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/error/category_win.cpp",
        "3rd/bee.lua/bee/utility/unicode_win.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_helper.cpp",
        "include/base/hook/inline.cpp",
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
