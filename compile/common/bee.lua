local lm = require "luamake"

lm.rootdir = "../../3rd/bee.lua"

lm:lua_source "source_bee" {
    sources = "3rd/lua-seri/lua-seri.c",
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
        end
    end
    return t
end

lm:source_set "source_bee" {
    includes = ".",
    sources = "bee/**/*.cpp",
    msvc = lm.analyze and {
        flags = "/analyze",
    },
    gcc = lm.analyze and {
        flags = {
            "-fanalyzer",
            "-Wno-analyzer-use-of-uninitialized-value"
        },
    },
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
        sources = need {
            "linux",
            "posix",
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

lm:lua_source "source_bee" {
    includes = ".",
    defines = {
        lm.EXE ~= "lua" and "BEE_STATIC",
    },
    sources = "binding/*.cpp",
    msvc = lm.analyze and {
        flags = "/analyze",
    },
    gcc = lm.analyze and {
        flags = {
            "-fanalyzer",
            "-Wno-analyzer-use-of-uninitialized-value"
        },
    },
    windows = {
        defines = "_CRT_SECURE_NO_WARNINGS",
        links = {
            "advapi32",
            "ws2_32",
            "ole32",
            "user32",
            "version",
            "wbemuuid",
            "oleAut32",
            "shell32",
            "ntdll",
            "synchronization",
        },
    },
    mingw = {
        links = {
            "uuid",
            "stdc++fs"
        }
    },
    linux = {
        sources = "!binding/lua_unicode.cpp",
        links = "stdc++fs",
        ldflags = "-pthread"
    },
    macos = {
        sources = "!binding/lua_unicode.cpp",
        frameworks = {
            "Foundation",
            "CoreFoundation",
            "CoreServices",
        }
    },
    ios = {
        sources = {
            "!binding/lua_unicode.cpp",
            "!binding/lua_filewatch.cpp",
        },
        frameworks = "Foundation",
    },
    android = {
        sources = "!binding/lua_unicode.cpp",
    },
    netbsd = {
        sources = {
            "!binding/lua_unicode.cpp",
        },
        links = ":libinotify.a",
        linkdirs = "/usr/pkg/lib",
        ldflags = "-pthread"
    },
    freebsd = {
        sources = {
            "!binding/lua_unicode.cpp",
        },
        links = "inotify",
        linkdirs = "/usr/local/lib",
        ldflags = "-pthread"
    },
    openbsd = {
        sources = {
            "!binding/lua_unicode.cpp",
        },
        links = ":libinotify.a",
        linkdirs = "/usr/local/lib/inotify",
        ldflags = "-pthread"
    },
}

lm:source_set "source_lua" {
    sources = "3rd/lua/utf8_crt.c",
}

lm:source_set "source_lua" {
    sources = {
        "3rd/lua/onelua.c",
        "3rd/lua/linit.c",
    },
    defines = "MAKE_LIB",
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
        sources = ("3rd/lua/fast_setjmp_%s.s"):format(lm.arch)
    },
    gcc = {
        flags = "-Wno-maybe-uninitialized",
    }
}

lm:source_set "source_bootstrap" {
    deps = { "source_bee", "source_lua" },
    includes = { "3rd/lua", "." },
    sources = "bootstrap/main.cpp",
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
