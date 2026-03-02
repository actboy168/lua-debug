local lm = require "luamake"

lm.rootdir = "../../3rd/bee.lua"

lm:source_set "source_bee" {
    includes = "3rd/lua55",
    sources = "3rd/lua-seri/lua-seri.cpp",
    msvc = {
        flags = "/wd4244"
    }
}

lm:source_set "source_bee" {
    sources = "3rd/fmt/format.cc",
}

local OS = {
    "win",
    "posix",
    "osx",
    "linux",
    "bsd",
}

local function need(lst)
    local map = {}
    if type(lst) == "table" then
        for _, v in ipairs(lst) do
            map[v] = true
        end
    else
        map[lst] = true
    end
    local t = {}
    for _, v in ipairs(OS) do
        if not map[v] then
            t[#t+1] = "!bee/**/*_"..v..".cpp"
            t[#t+1] = "!bee/"..v.."/**/*.cpp"
        end
    end
    return t
end

lm:source_set "source_bee" {
    includes = {
        ".",
        "3rd/lua55/",
    },
    sources = "bee/**/*.cpp",
    windows = {
        sources = need "win"
    },
    macos = {
        sources = {
            need {
                "osx",
                "posix",
            }
        }
    },
    ios = {
        sources = {
            "!bee/filewatch/**/",
            need {
                "osx",
                "posix",
            }
        }
    },
    linux = {
        sources = {
            need {
                "linux",
                "posix",
            }
        }
    },
    android = {
        sources = need {
            "linux",
            "posix",
        }
    },
    netbsd = {
        sysincludes = "/usr/pkg/include",
        sources = need {
            "bsd",
            "posix",
        }
    },
    freebsd = {
        sysincludes = "/usr/local/include",
        sources = need {
            "bsd",
            "posix",
        }
    },
    openbsd = {
        sysincludes = "/usr/local/include/inotify",
        sources = need {
            "bsd",
            "posix",
        }
    }
}

lm:source_set "source_bee" {
    includes = { "3rd/lua55", "." },
    defines = {
        lm.EXE ~= "lua" and "BEE_STATIC",
    },
    sources = {
        "binding/*.cpp",
        "3rd/lua-patch/bee_newstate.c",
    },
    windows = {
        defines = "_CRT_SECURE_NO_WARNINGS",
        sources = {
            "binding/port/lua_windows.cpp",
        },
        links = {
            "ntdll",
            "ws2_32",
            "ole32",
            "user32",
            "version",
            "synchronization",
            lm.arch == "x86" and "dbghelp",
        },
    },
    mingw = {
        links = {
            "uuid",
            "stdc++fs"
        }
    },
    linux = {
        ldflags = "-pthread -l:libbfd.a -l:libiberty.a -l:libsframe.a -l:libzstd.so -l:libz.so",
        links = {
            "stdc++fs",
            "unwind",
            "gcc_s"
        }
    },
    macos = {
        frameworks = {
            "Foundation",
            "CoreFoundation",
            "CoreServices",
        }
    },
    ios = {
        sources = {
            "!binding/lua_filewatch.cpp",
        },
        frameworks = "Foundation",
    },
    netbsd = {
        links = ":libinotify.a",
        linkdirs = "/usr/pkg/lib",
        ldflags = "-pthread"
    },
    freebsd = {
        links = "inotify",
        linkdirs = "/usr/local/lib",
        ldflags = "-pthread"
    },
    openbsd = {
        links = ":libinotify.a",
        linkdirs = "/usr/local/lib/inotify",
        ldflags = "-pthread"
    },
}

lm:source_set "source_lua" {
    includes = ".",
    sources = "3rd/lua-patch/bee_utf8_crt.cpp",
}

lm:source_set "source_lua" {
    includes = "3rd/lua55",
    sources = {
        "3rd/lua55/onelua.c",
    },
    defines = {
        "MAKE_LIB",
        "LUA_COMPAT_LOOPVAR",
    },
    windows = {
        defines = "LUA_BUILD_AS_DLL",
    },
    macos = {
        defines = "LUA_USE_MACOSX",
        visibility = "default",
    },
    linux = {
        defines = "LUA_USE_LINUX",
        visibility = "default",
    },
    netbsd = {
        defines = "LUA_USE_LINUX",
        visibility = "default",
    },
    freebsd = {
        defines = "LUA_USE_LINUX",
        visibility = "default",
    },
    openbsd = {
        defines = "LUA_USE_LINUX",
        visibility = "default",
    },
    android = {
        defines = "LUA_USE_LINUX",
        visibility = "default",
    },
    msvc = {
        flags = {
            "/wd4267",
            "/wd4334",
        },
        sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch)
    },
    gcc = {
        flags = "-Wno-maybe-uninitialized",
    }
}

lm:source_set "source_bootstrap" {
    deps = {
        "source_bee",
        "source_lua",
    },
    includes = { "3rd/lua55", "." },
    sources = {
        "bootstrap/main.cpp",
        "bootstrap/bootstrap_init.cpp",
    },
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" }
    },
    netbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    freebsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    openbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    android = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" }
    }
}
