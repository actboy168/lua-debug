local lm = require "luamake"
local platform = lm.runtime_platform

lm.cxx = "c++17"

lm.defines = "_WIN32_WINNT=0x0601"
lm.builddir = ("build/%s/%s"):format(platform, lm.mode)

require "compile.common.runtime"

lm:msvc_copydll "copy_vcredist" {
    type = "vcrt",
    output = 'publish/vcredist/'..platform
}

lm:source_set 'detours' {
    rootdir = "3rd/detours/src",
    sources = {
        "*.cpp",
        "!uimports.cpp"
    }
}

local ArchAlias = {
    ["win32-x64"] = "x64",
    ["win32-ia32"] = "x86",
}

lm:lua_library ('launcher.'..ArchAlias[platform]) {
    bindir = "publish/bin/",
    export_luaopen = "off",
    deps = "detours",
    includes = {
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
        "3rd/detours/src",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/utility/unicode_win.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_win.cpp",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/*.cpp",
    },
    defines = {
        "BEE_INLINE",
        "_CRT_SECURE_NO_WARNINGS",
        "LUA_DLL_VERSION=lua54"
    },
    links = {
        "ws2_32",
        "user32",
        "shell32",
        "ole32",
        "delayimp",
    },
    ldflags = '/DELAYLOAD:lua54.dll',
}

lm:phony "launcher" {
    deps = 'launcher.'..ArchAlias[platform]
}
