local lm = require "luamake"
if lm.os == "windows" then
    require "compile.windows.make"
    return
elseif lm.os == "macos" then
    require "compile.macos.make"
    return
end

if not lm.arch then
    local function shell(command)
        local f = assert(io.popen(command, 'r'))
        local r = f:read '*l'
        f:close()
        return r:lower()
    end
    lm.arch = shell "uname -m"
end

lm.builddir = ("build/%s/%s/%s"):format(lm.os, lm.arch, lm.mode)
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

require "compile.common.runtime"

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:build 'install-lua-debug' {
    '$luamake', 'lua', 'compile/install_lua-debug.lua', lm.builddir..'/bin',
    deps = "lua-debug"
}

lm:build 'install-runtime' {
    '$luamake', 'lua', 'compile/install_runtime.lua', lm.builddir..'/bin', lm.arch,
    deps = "runtime",
}

lm:default {
    'copy_extension',
    'update_version',
    "install-lua-debug",
    'install-runtime',
}
