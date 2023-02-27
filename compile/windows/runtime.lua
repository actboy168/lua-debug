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
        "3rd/bee.lua/bee/platform/win/version_win.cpp",
        "3rd/bee.lua/bee/platform/win/unicode_win.cpp",
        "3rd/bee.lua/bee/platform/win/unlink_win.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_win.cpp",
        "3rd/bee.lua/bee/net/endpoint.cpp",
        "3rd/bee.lua/bee/net/socket.cpp",
        "3rd/bee.lua/bee/nonstd/3rd/os.cc",
        "3rd/bee.lua/bee/nonstd/3rd/format.cc",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/old/*.cpp",
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
        "version",
        "ntdll",
    },
    ldflags = '/DELAYLOAD:lua54.dll',
}

lm:phony "launcher" {
    deps = 'launcher.'..ArchAlias[platform]
}
