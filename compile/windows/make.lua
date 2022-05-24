local lm = require "luamake"

lm.arch = "x86"
lm.runtime_platform = "win32-ia32"
require "compile.windows.runtime"

lm:rule "build_x86_64" {
    "$luamake",
    "-C", lm.workdir,
    "-f", "compile/windows/runtime.lua",
    "-builddir", "build/win32-x64/"..lm.mode,
    "-mode", lm.mode,
    "-arch", "x86_64",
    "-runtime_platform", "win32-x64",
    pool = "console",
}

lm:build "x86_64" {
    rule = "build_x86_64"
}

lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm.EXE_RESOURCE = "../../compile/windows/lua-debug.rc"
lm:import "3rd/bee.lua/make.lua"
lm:lua_dll 'inject' {
    bindir = "publish/bin/",
    deps = "lua54",
    defines = "BEE_INLINE",
    includes = {
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
        "3rd/wow64ext/src",
    },
    sources = {
        "src/process_inject/injectdll.cpp",
        "src/process_inject/inject.cpp",
        "3rd/wow64ext/src/wow64ext.cpp",
        "3rd/bee.lua/bee/utility/unicode_win.cpp",
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
    "x86_64"
}
