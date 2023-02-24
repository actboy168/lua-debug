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

lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm.EXE_RESOURCE = "../../compile/windows/lua-debug.rc"
lm:import "3rd/bee.lua/make.lua"
lm:lua_dll 'inject' {
    bindir = "publish/bin",
    deps = "lua54",
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
if lm.mode == "debug" then
    lm:executable "test_delayload" {
        sources = "test/delayload.cpp",
        includes = {"src/launcher","3rd/lua/lua54/"},
    }
    lm:phony "tests" {
        deps = {"test_frida", "test_delayload"}
    }
end

lm:default {
    "common",
    "copy_vcredist",
    "lua-debug",
    "inject",
    "launcher",
    "runtime",
    "x86_64",
    lm.mode == "debug" and "tests"
}
