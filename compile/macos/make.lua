local lm = require "luamake"

if lm.platform == "darwin-arm64" then
    lm.target = "arm64-apple-macos11"
else
    lm.target = "x86_64-apple-macos10.12"
end

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/macos"
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
        pool = "console",

    }
    lm:build "x86_64" {
        rule = "luamake",
    }
end

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/macos/main.lua",
}

lm:default {
    "copy_extension",
    "update_version",
    "copy_bootstrap",
    "lua-debug",
    "runtime",
    lm.platform == "darwin-arm64" and "x86_64"
}
