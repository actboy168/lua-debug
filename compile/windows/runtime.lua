local lm = require "luamake"
local platform = lm.runtime_platform

lm.cxx = "c++17"

lm.defines = "_WIN32_WINNT=0x0601"
lm.builddir = ("build/%s/%s"):format(platform, lm.mode)

require "compile.common.runtime"
require "compile.common.frida"

lm:msvc_copydll "copy_vcredist" {
    type = "vcrt",
    output = 'publish/vcredist/'..platform
}

lm:source_set "std_format" {
    includes = "3rd/bee.lua/bee/nonstd/3rd/fmt",
    sources = {
        "3rd/bee.lua/bee/nonstd/3rd/format.cc",
        "3rd/bee.lua/bee/nonstd/3rd/os.cc",
    }
}

local ArchAlias = {
    ["win32-x64"] = "x64",
    ["win32-ia32"] = "x86",
}

lm:source_set ("launcher_hook_luajit"){
    includes = {"3rd/lua/luajit/src", "3rd/frida_gum/gumpp"},
    sources = "src/launcher/hook/luajit_listener.cpp",
}

lm:lua_library ('launcher.'..ArchAlias[platform]) {
    bindir = "publish/bin",
    export_luaopen = "off",
    deps = {"std_format", "frida", "launcher_hook_luajit"},
    includes = {
        "3rd/bee.lua",
        "3rd/lua/lua54",
        "3rd/frida_gum/gumpp",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/platform/win/unicode_win.cpp",
        "3rd/bee.lua/bee/platform/win/unlink_win.cpp",
        "3rd/bee.lua/bee/platform/win/version_win.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_win.cpp",
        "3rd/bee.lua/bee/net/endpoint.cpp",
        "3rd/bee.lua/bee/net/socket.cpp",
        "src/launcher/*.cpp",
        "src/launcher/hook/*.cpp",
        "src/launcher/utility/*.cpp",
        "!src/launcher/hook/luajit_listener.cpp",
    },
    defines = {
        "BEE_INLINE",
        "_CRT_SECURE_NO_WARNINGS",
    },
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
}

lm:phony "launcher" {
    deps = 'launcher.'..ArchAlias[platform]
}
