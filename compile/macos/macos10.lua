local lm = require "luamake"
lm.builddir = ("build/macos/%s/x86_64"):format(lm.mode)
lm.EXE_DIR = "publish/bin/macos"
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

lm.arch = lm.arch or "x86_64"
assert(loadfile("compile/common/runtime.lua"))(lm.os.."/"..lm.arch)

lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/macos/main.lua",
}
lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}
lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}
lm:default {
    "copy_bootstrap",
    "lua-debug",
    'copy_extension',
    'update_version',
}
