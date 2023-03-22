local lm = require "luamake"

lm.arch = "x86"
lm.runtime_platform = "win32-ia32"
require "compile.windows.runtime"
require "compile.common.run_luamake"

lm:build "x86_64" {
    rule = "run_luamake",
    inputs = "compile/windows/runtime.lua",
    args = {
        "-builddir", "build/win32-x64/"..lm.mode,
        "-runtime_platform", "win32-x64",
        "-arch", "x86_64"
    },
}

lm:import "3rd/bee.lua/make.lua"
require "compile.common.lua-debug"

if lm.enable_sanitize then
    lm:msvc_copydll "copy_asan_to_bin" {
        type = "asan",
        output = "publish/bin/",
    }
end

lm:lua_dll 'inject' {
    bindir = "publish/bin",
    defines = "BEE_INLINE",
    includes = {
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
        "3rd/wow64ext/src",
    },
    sources = {
        "src/process_inject/windows/*.cpp",
        "3rd/wow64ext/src/wow64ext.cpp",
        "3rd/bee.lua/bee/platform/win/unicode_win.cpp",
    },
    links = "advapi32",
}

lm:default {
    "common",
    "copy_vcredist",
    "lua-debug",
    "inject",
    "launcher",
    "runtime",
    "x86_64",
    lm.enable_sanitize and "copy_asan_to_bin",
}
