local lm = require "luamake"

lm.builddir = ("build/test/%s"):format(lm.mode)

local arch = lm.arch
if not arch then
    arch = lm.runtime_platform:match "[%w]+-([%w]+)"
end

if arch == "x64" then
    arch = "x86_64"
end

require 'compile.common.frida'

lm:executable "test_frida" {
    bindir = lm.builddir.."/tests",
    deps = "frida",
    includes = "3rd/frida_gum/gumpp",
    sources = "test/interceptor.cpp",
}

lm:executable "test_symbol" {
    bindir = lm.builddir.."/tests",
    deps = "frida",
    sources = "test/symbol/*.c",
    includes = {
        "3rd/frida_gum/"..lm.os.."-"..arch,
        "3rd/frida_gum/gumpp"
    },
}

lm:executable "test_delayload" {
    sources = "test/delayload.cpp",
    includes = {
        "src/launcher",
        "3rd/lua/lua54"
    },
}

lm:executable "testwaitdll" {
    sources = "test/waitdll.cpp",
    includes = { "3rd/lua/lua51" },
}

lm:executable "test_thunk" {
    sources = "test/thunk.cpp",
    includes = { "src/luadebug" },
}

lm:default {
    "test_frida",
    "test_delayload",
    "test_symbol",
    "testwaitdll",
    "test_thunk"
}
