local lm = require "luamake"
lm.builddir = ("build/macos/%s/x86_64"):format(lm.mode)

lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

require "compile.common.runtime"

lm:build 'install_lua-debug' {
    '$luamake', 'lua', 'compile/install_lua-debug.lua', lm.builddir.."/bin",
    deps = "lua-debug"
}
lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}
lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}
lm:default {
    'copy_extension',
    'update_version',
}
