local lm = require "luamake"

require 'compile.common.detect_platform'

local arch = lm.arch

local dir = "3rd/frida_gum/"..lm.os.."-"..arch
lm:source_set "frida" {
    includes = dir,
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
