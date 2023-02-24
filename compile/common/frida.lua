local lm = require "luamake"

require 'compile.common.detect_platform'

local arch = lm.arch
if not arch then
    arch = lm.runtime_platform:match "[%w]+-([%w]+)"
end

lm:build "download_frida_sdk" {
    "$luamake",
    "lua",
    "compile/update_frida_gum.lua"
}

local dir = "3rd/frida_gum/"..lm.os.."-"..arch
lm:source_set "frida" {
    includes = dir,
    deps = "download_frida_sdk",
    defines = {"GUMPP_STATIC"},
    windows = {
        defines = "HAVE_WINDOWS",
        links = {"kernel32","user32","gdi32","winspool","comdlg32","advapi32","shell32","ole32","oleaut32","uuid","odbc32","odbccp32"},
    },
    sources = {
        "3rd/frida_gum/gumpp/*.cpp",
    },
    links = {
        dir.."/frida-gum"
    }
}

if lm.mode == "debug" then
    lm:executable("test_frida") {
        bindir = lm.builddir.."/tests",
        deps = "frida",
        includes = "3rd/frida_gum/gumpp",
        sources = "test/interceptor.cpp",
    }
end
