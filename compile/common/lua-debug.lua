local lm = require "luamake"

if lm.os == "windows" then
    lm:source_set "source_inject" {
        bindir = "publish/bin",
        includes = {
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua",
            "3rd/wow64ext/src",
        },
        sources = {
            "src/process_inject/windows/*.cpp",
            "3rd/wow64ext/src/wow64ext.cpp",
        },
        msvc = {
            flags = "/utf-8",
        },
        links = "advapi32",
    }
end

lm:executable "lua-debug" {
    bindir = "publish/bin/",
    deps = "source_bootstrap",
    windows = {
        deps = "source_inject",
        sources = {
            "compile/windows/lua-debug.rc"
        }
    },
    msvc = {
        ldflags = "/IMPLIB:$obj/lua-debug.lib"
    },
    mingw = {
        ldflags = "-Wl,--out-implib,$obj/lua-debug.lib"
    },
    linux = {
        crt = "static",
    },
    netbsd = {
        crt = "static",
    },
    freebsd = {
        crt = "static",
    },
    openbsd = {
        crt = "static",
    },
}
