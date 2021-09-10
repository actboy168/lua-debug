local lm = require "luamake"

lm.builddir = ("build/macos/%s"):format(lm.mode)
lm.EXE_NAME = "lua-debug"
lm.universal = "on"
lm:import "3rd/bee.lua/make.lua"

lm:build "arm64" {
    "$luamake", "-target", "arm64-apple-macos11",
    pool = "console",
}
lm:build "x86_64" {
    "$luamake", "-target", "x86_64-apple-macos10.12",
    pool = "console",
}
lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}
lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}
lm:build 'install-lua-debug' {
    '$luamake', 'lua', 'compile/install_lua-debug.lua', ("build/macos/%s/bin"):format(lm.mode),
    deps = "lua-debug-universal"
}

lm:default {
    "copy_extension",
    "update_version",
    "install-lua-debug",
    "arm64",
    "x86_64"
}
