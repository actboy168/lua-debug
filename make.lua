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
        local f <close> = assert(io.popen(command, 'r'))
        return f:read 'l':lower()
    end
    lm.arch = shell "uname -m"
end

lm.builddir = ("build/%s/%s/%s"):format(lm.os, lm.arch, lm.mode)
lm.EXE_DIR = "publish/bin/"..lm.os
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

require "compile.common.runtime"

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:copy 'copy_bootstrap' {
    input = "extension/script/bootstrap.lua",
    output = "publish/bin/"..lm.os.."/main.lua",
}

lm:default {
    "copy_extension",
    "update_version",
    "copy_bootstrap",
    "lua-debug",
    "runtime",
}
