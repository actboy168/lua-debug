local lm = require "luamake"

lm:executable "lua-debug" {
    bindir = "publish/bin/",
    deps = "source_bootstrap",
    windows = {
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
