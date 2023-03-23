local lm = require "luamake"

require 'compile.common.detect_platform'

local arch = lm.arch
if not arch then
    arch = lm.runtime_platform:match "[%w]+-([%w]+)"
end

if arch == "x64" then
    arch = "x86_64"
end

local dir = "3rd/frida_gum/"..lm.os.."-"..arch
lm:source_set "frida" {
    includes = {dir, "3rd/frida_gum/gumpp"},
    defines = {"GUMPP_STATIC"},
    windows = {
        defines = {"HAVE_WINDOWS", "_CRT_SECURE_NO_WARNINGS"},
        links = {"kernel32","user32","gdi32","winspool","comdlg32","advapi32","shell32","ole32","oleaut32","uuid","odbc32","odbccp32"},
        sources = "3rd/frida_gum/gumpp/src/internal/*.c",
        flags = {
            '/wd4819',
            '/wd5051',
        }
    },
    macos = {
        frameworks = {
            "CoreFoundation",
        }
    },
    sources = {
        "3rd/frida_gum/gumpp/src/*.cpp",
    },
    linkdirs = dir,
    links = "frida-gum",
}
