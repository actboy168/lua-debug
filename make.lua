local lm = require "luamake"

require "compile.common.detect_platform"

lm.compile_commands = "build"
lm.c = "c11"
lm.cxx = "c++17"

lm.mode = "debug"

lm.msvc = {
    flags = "/wd5105"
}

if lm.mode == "debug" then
    lm.enable_sanitize = true
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

if lm.test then
    require "compile.test.make"
    return
end

require "compile.common.make"

if lm.os == "windows" then
    require "compile.windows.make"
elseif lm.os == "macos" then
    require "compile.macos.make"
else
    require "compile.linux.make"
end
