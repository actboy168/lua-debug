local lm = require "luamake"

lm.builddir = ("build/macos/%s"):format(lm.mode)
lm.EXE_DIR = "publish/bin/macos"
lm.EXE_NAME = "lua-debug"
lm.universal = "on"
lm:import "3rd/bee.lua/make.lua"

lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/macos/main.lua",
}
lm:build "runtiume-arm64" {
    "$luamake", "-target", "arm64-apple-macos11",
    pool = "console",
}
lm:build "runtiume-x86_64" {
    "$luamake", "-target", "x86_64-apple-macos10.12",
    pool = "console",
}
lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}
lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:default {
    "copy_extension",
    "update_version",
    "copy_bootstrap",
    "lua-debug-universal",
    "runtiume-arm64",
    "runtiume-x86_64"
}
