local lm = require "luamake"

require "compile.common.detect_platform"

lm.compile_commands = "build"
lm.cxx = "c++17"
lm.luajit = "on"

require "compile.common.make"

if lm.cross_compiler then
    lm:build "buildvm" {
        "$luamake",
        "-C", lm.workdir,
        "-f", "compile/luajit/make_buildtools.lua",
        "-cross", "arm64",
        "-bindir",lm.bindir,
        pool = "console",
    }
else
    require "compile.luajit.make_buildtools"
end

if lm.os == "windows" then
    require "compile.windows.make"
elseif lm.os == "macos" then
    require "compile.macos.make"
else
    require "compile.linux.make"
end
