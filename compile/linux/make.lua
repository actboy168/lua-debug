local lm = require "luamake"

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

lm.runtime_platform = lm.platform
require "compile.common.runtime"

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/main.lua",
}

lm:default {
    "copy_extension",
    "update_version",
    "copy_bootstrap",
    "lua-debug",
    "runtime",
}
