local lm = require "luamake"

lm.arch = "x86"
lm.runtime_platform = "win32-ia32"
require "compile.windows.runtime"

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}
lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}
lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/windows/main.lua",
}
lm:build "copy_vcredist" {
    "$luamake", "lua", "compile/windows/copy_vcredist.lua"
}

if lm.platform == "win32-x64" then
    lm:rule "luamake" {
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
        rule = "luamake"
    }
end

lm.EXE_DIR = "publish/bin/windows"
lm.EXE_NAME = "lua-debug"
lm.EXE_RESOURCE = "../../compile/windows/lua-debug.rc"
lm:import "3rd/bee.lua/make.lua"
lm:lua_dll 'inject' {
    bindir = "publish/bin/windows",
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
    "update_version",
    "copy_extension",
    "copy_bootstrap",
    "copy_vcredist",
    "lua-debug",
    "inject",
    "launcher",
    "runtime",
    lm.platform == "win32-x64" and "x86_64",
}
