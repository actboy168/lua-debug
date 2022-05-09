local lm = require "luamake"

if lm.platform == "darwin-arm64" then
    lm.target = "arm64-apple-macos11"
else
    lm.target = "x86_64-apple-macos10.12"
end

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

lm.runtime_platform = lm.platform
require "compile.common.runtime"

if lm.platform == "darwin-arm64" then
    lm:rule "luamake" {
        "$luamake",
        "-C", lm.workdir,
        "-f", "compile/common/runtime.lua",
        "-builddir", "build/darwin-x64/"..lm.mode,
        "-mode", lm.mode,
        "-target", "x86_64-apple-macos10.12",
        "-runtime_platform", "darwin-x64",
        "-luajit", lm.luajit,
        pool = "console",

    }
    lm:build "x86_64" {
        rule = "luamake",
    }
end

lm:default {
    "common",
    "lua-debug",
    "runtime",
    lm.platform == "darwin-arm64" and "x86_64"
}
