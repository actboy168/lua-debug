local lm = require "luamake"
local fs = require "bee.filesystem"

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

local bindir = "publish/bin/"..ArchAlias[platform]

lm:source_set ("launcher_hook_luajit"){
    includes = {"3rd/lua/luajit/src", "3rd/frida_gum/gumpp"},
    sources = "src/launcher/hook/luajit_listener.cpp",
}

lm:lua_library ('launcher.'..ArchAlias[platform]) {
    bindir = bindir,
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
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_win.cpp",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/*.cpp",
        "src/launcher/hook/*.cpp",
        "!src/launcher/hook/luajit_listener.cpp",
        "src/launcher/symbol_resolver/*.cpp",
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
}

local DbghelpPath = {
    ["win32-x64"] = "c:/windows/system32/",
    ["win32-ia32"] = "c:/windows/syswow64/",
}

fs.create_directories(bindir)

fs.copy_file(fs.path(DbghelpPath[platform]) / "dbghelp.dll", fs.current_path() / fs.path(bindir) / "dbghelp.dll", fs.copy_options.overwrite_existing)

lm:phony "launcher" {
    deps = 'launcher.'..ArchAlias[platform]
}
