local lm = require "luamake"

require "compile.common.detect_platform"

lm.compile_commands = "build"
lm.c = "c11"
lm.cxx = "c++17"

lm.msvc = {
    flags = "/wd5105"
}

require "compile.common.make"

if lm.os == "windows" then
    require "compile.windows.make"
elseif lm.os == "macos" then
    require "compile.macos.make"
else
    require "compile.linux.make"
end
