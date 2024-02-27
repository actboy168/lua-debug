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

lm:import "compile/common/bee.lua"
require "compile.common.lua-debug"

lm:default {
    "common",
    lm.mode ~= "debug" and "copy_vcredist",
    "lua-debug",
    "launcher",
    "runtime",
    "x86_64",
}
