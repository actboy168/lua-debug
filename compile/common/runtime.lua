local lm = require "luamake"

require "compile.common.detect_platform"
require "compile.luadbg.build"

local runtimes = {}

lm.cxx = "c++17"

local bindir = "publish/runtime/"..lm.runtime_platform

lm:source_set 'onelua' {
    includes = {
        "3rd/bee.lua/3rd/lua",
        "src/luadebug/",
    },
    sources = "src/luadebug/luadbg/onelua.c",
    msvc = {
        sources = ("3rd/bee.lua/3rd/lua/fast_setjmp_%s.s"):format(lm.arch)
    },
    linux = {
        flags = "-fPIC"
    },
    netbsd = {
        flags = "-fPIC"
    },
    freebsd = {
        flags = "-fPIC"
    },
    gcc = {
        flags = "-Wno-maybe-uninitialized"
    }
}

lm:source_set 'luadbg' {
    deps = "onelua",
    includes = {
        "src/luadebug",
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
    },
    sources = {
        "src/luadebug/luadbg/*.cpp",
    },
    msvc = {
        flags = "/utf-8",
    },
    linux = {
        flags = "-fPIC"
    },
    netbsd = {
        flags = "-fPIC"
    },
    freebsd = {
        flags = "-fPIC"
    },
    windows = {
        defines = {
            "_CRT_SECURE_NO_WARNINGS",
            "_WIN32_WINNT=0x0601",
        },
    }
}

lm:source_set 'luadbg-compatible' {
    defines = {
        "LUADBG_DISABLE",
    },
    includes = {
        "src/luadebug",
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
    },
    sources = {
        "src/luadebug/luadbg/*.cpp",
        "3rd/bee.lua/3rd/lua-seri/lua-seri.c",
    },
    msvc = {
        flags = "/utf-8",
    },
    linux = {
        flags = "-fPIC"
    },
    netbsd = {
        flags = "-fPIC"
    },
    freebsd = {
        flags = "-fPIC"
    },
    windows = {
        defines = {
            "_CRT_SECURE_NO_WARNINGS",
            "_WIN32_WINNT=0x0601",
        },
    }
}

local compat <const> = {
    ["lua51"]      = "compat/5x",
    ["lua52"]      = "compat/5x",
    ["lua53"]      = "compat/5x",
    ["lua54"]      = "compat/5x",
    ["lua-latest"] = "compat/5x",
    ["lua-compatible"] = "compat/5x",
    ["luajit"]     = "compat/jit"
}
for _, luaver in ipairs { "lua51", "lua52", "lua53", "lua54", "luajit", "lua-latest", "lua-compatible" } do
    runtimes[#runtimes + 1] = luaver.."/lua"
    if lm.os == "windows" then
        runtimes[#runtimes + 1] = luaver.."/"..luaver
    end
    if luaver == "luajit" then
        if lm.os == "windows" then
            require "compile.luajit.make_windows"
        else
            if lm.cross_compile then
                require "compile.common.run_luamake"
                lm:build "buildvm" {
                    rule = "run_luamake",
                    inputs = "compile/luajit/make_buildtools.lua",
                    args = {
                        "-bindir", lm.bindir,
                        "-runtime_platform", lm.runtime_platform,
                    },
                }
            else
                require "compile.luajit.make_buildtools"
            end
            require "compile.luajit.make"
        end
    elseif luaver == "lua-compatible" then
        if lm.os == "windows" then
            lm:shared_library(luaver.."/"..luaver) {
                rootdir = '3rd/lua/lua-latest',
                bindir = bindir,
                includes = {
                    '..',
                },
                sources = {
                    "*.c",
                    "!lua.c",
                    "!luac.c",
                },
                defines = {
                    "LUA_BUILD_AS_DLL",
                    "LUA_VERSION_LATEST",
                }
            }

            lm:executable(luaver..'/lua') {
                rootdir = '3rd/lua/lua-latest',
                bindir = bindir,
                output = "lua",
                deps = luaver..'/'..luaver,
                includes = {
                    '..',
                },
                sources = {
                    "lua.c",
                    "../../../compile/windows/lua-debug.rc",
                },
                defines = {
                    "LUA_VERSION_LATEST",
                }
            }
        else
            lm:executable(luaver..'/lua') {
                rootdir = '3rd/lua/lua-latest',
                bindir = bindir,
                includes = {
                    '.',
                    '..',
                },
                sources = {
                    "*.c",
                    "!luac.c",
                },
                defines = {
                    "LUA_VERSION_LATEST",
                },
                visibility = "default",
                links = "m",
                linux = {
                    defines = "LUA_USE_LINUX",
                    links = { "pthread", "dl" },
                    ldflags = "-Wl,-E",
                },
                netbsd = {
                    defines = "LUA_USE_LINUX",
                    links = "pthread",
                    ldflags = "-Wl,-E",
                },
                freebsd = {
                    defines = "LUA_USE_LINUX",
                    links = "pthread",
                    ldflags = "-Wl,-E",
                },
                android = {
                    defines = "LUA_USE_LINUX",
                    links = "dl",
                },
                macos = {
                    defines = {
                        "LUA_USE_MACOSX",
                    },
                }
            }
        end
    else
        if lm.os == "windows" then
            lm:shared_library(luaver.."/"..luaver) {
                rootdir = '3rd/lua/'..luaver,
                bindir = bindir,
                includes = {
                    '..',
                },
                sources = {
                    "*.c",
                    "!lua.c",
                    "!luac.c",
                },
                defines = {
                    "LUA_BUILD_AS_DLL",
                    luaver == "lua51" and "_CRT_SECURE_NO_WARNINGS",
                    luaver == "lua52" and "_CRT_SECURE_NO_WARNINGS",
                    luaver == "lua-latest" and "LUA_VERSION_LATEST",
                }
            }

            lm:executable(luaver..'/lua') {
                rootdir = '3rd/lua/'..luaver,
                bindir = bindir,
                output = "lua",
                deps = luaver..'/'..luaver,
                includes = {
                    '..',
                },
                sources = {
                    "lua.c",
                    "../../../compile/windows/lua-debug.rc",
                },
                defines = {
                    luaver == "lua51" and "_CRT_SECURE_NO_WARNINGS",
                    luaver == "lua52" and "_CRT_SECURE_NO_WARNINGS",
                    luaver == "lua-latest" and "LUA_VERSION_LATEST",
                }
            }
        else
            lm:executable(luaver..'/lua') {
                rootdir = '3rd/lua/'..luaver,
                bindir = bindir,
                includes = {
                    '.',
                    '..',
                },
                sources = {
                    "*.c",
                    "!luac.c",
                },
                defines = {
                    luaver == "lua51" and "_XOPEN_SOURCE=600",
                    luaver == "lua52" and "_XOPEN_SOURCE=600",
                    luaver == "lua-latest" and "LUA_VERSION_LATEST",
                },
                visibility = "default",
                links = "m",
                linux = {
                    defines = "LUA_USE_LINUX",
                    links = { "pthread", "dl" },
                    ldflags = "-Wl,-E",
                },
                netbsd = {
                    defines = "LUA_USE_LINUX",
                    links = "pthread",
                    ldflags = "-Wl,-E",
                },
                freebsd = {
                    defines = "LUA_USE_LINUX",
                    links = "pthread",
                    ldflags = "-Wl,-E",
                },
                android = {
                    defines = "LUA_USE_LINUX",
                    links = "dl",
                },
                macos = {
                    defines = {
                        "LUA_USE_MACOSX",
                        luaver == "lua51" and "LUA_USE_DLOPEN",
                    },
                }
            }
        end
    end

    local luaSrcDir; do
        if luaver == "luajit" then
            luaSrcDir = "3rd/lua/luajit/src"
        elseif luaver == "lua-compatible" then
            luaSrcDir = "3rd/lua/lua-latest"
        else
            luaSrcDir = "3rd/lua/"..luaver
        end
    end

    runtimes[#runtimes + 1] = luaver.."/luadebug"
    lm:shared_library(luaver..'/luadebug') {
        bindir = bindir,
        deps = {
            luaver == "lua-compatible" and "luadbg-compatible" or "luadbg",
            "compile_to_luadbg",
        },
        defines = {
            luaver == "lua-latest" and "LUA_VERSION_LATEST",
            luaver == "lua-compatible" and "LUADBG_DISABLE",
        },
        includes = {
            luaSrcDir,
            "3rd/bee.lua/",
            "3rd/bee.lua/3rd/lua-seri",
            "src/luadebug/",
        },
        sources = {
            "src/luadebug/*.cpp",
            "src/luadebug/symbolize/*.cpp",
            "src/luadebug/thunk/*.cpp",
            "src/luadebug/util/*.cpp",
            "src/luadebug/"..compat[luaver].."/**/*.cpp",
        },
        msvc = {
            flags = "/utf-8",
        },
        windows = {
            deps = luaver..'/'..luaver,
            defines = {
                "_CRT_SECURE_NO_WARNINGS",
                "_WIN32_WINNT=0x0601",
                ("LUA_DLL_VERSION="..luaver)
            },
            links = {
                "version",
                "ws2_32",
                "user32",
                "shell32",
                "ole32",
                "delayimp",
                "dbghelp",
                "ntdll",
                "synchronization",
            },
            ldflags = {
                ("/DELAYLOAD:%s.dll"):format(luaver),
            },
        },
        macos = {
            frameworks = "Foundation"
        },
        linux = {
            links = "pthread",
            crt = "static",
        },
        netbsd = {
            links = "pthread",
        },
        freebsd = {
            links = "pthread",
            crt = "static",
        },
        android = {
            links = "m",
        }
    }
end

lm:phony "runtime" {
    input = runtimes
}
