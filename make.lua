local lm = require "luamake"

require "compile.common.detect_platform"

lm.compile_commands = "build"
lm.c = "c11"
lm.cxx = "c++17"

lm.msvc = {
    flags = "/wd5105"
}

if lm.enable_sanitize then
    lm.mode = "debug"
    lm.flags = "-fsanitize=address"
    lm.gcc = {
        ldflags = "-fsanitize=address"
    }
    lm.clang = {
        ldflags = "-fsanitize=address"
    }
    lm.msvc = {
        defines = "_DISABLE_STRING_ANNOTATION",
        flags = "/wd5105",
    }
end

require "compile.common.make"

if lm.os == "windows" then
    require "compile.windows.make"
elseif lm.os == "macos" then
    require "compile.macos.make"
else
    require "compile.linux.make"
end
