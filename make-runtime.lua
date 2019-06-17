local lm = require "luamake"
local platform = require "bee.platform"

lm.gcc = 'clang'
lm.gxx = 'clang++'

lm.arch = ARGUMENTS.arch or 'x64'

lm.bindir = ("build/%s/bin/%s"):format(lm.plat, lm.arch)
lm.objdir = ("build/%s/obj/%s"):format(lm.plat, lm.arch)

local BUILD_BIN = platform.OS ~= "Windows" or lm.arch ~= "x64"

if BUILD_BIN then
    lm:import '3rd/bee.lua/make.lua'

    if platform.OS == "Windows" then
        lm:shared_library 'inject' {
            deps = {
                "bee",
                "lua54"
            },
            includes = {
                "src",
                "3rd/bee.lua",
                "3rd/wow64ext/src",
            },
            sources = {
                "src/base/hook/injectdll.cpp",
                "src/base/hook/replacedll.cpp",
                "src/process_inject/query_process.cpp",
                "src/process_inject/inject.cpp",
                "3rd/wow64ext/src/wow64ext.cpp",
            },
            links = "advapi32"
        }
    else
        lm:shared_library 'inject' {
            deps = {
                "bee",
            },
            includes = {
                "3rd/bee.lua/3rd/lua/src",
            },
            sources = {
                "src/process_inject/inject_osx.cpp",
            }
        }
    end
end

if platform.OS == "Windows" then
    lm:source_set 'detours' {
        rootdir = "3rd/detours/src",
        permissive = true,
        sources = {
            "*.cpp",
            "!uimports.cpp"
        }
    }

    lm:lua_library 'launcher' {
        export_luaopen = false,
        deps = {
            "detours",
        },
        includes = {
            "src",
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua/src",
        },
        sources = {
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/utility/unicode_win.cpp",
            "3rd/bee.lua/bee/utility/path_helper.cpp",
            "3rd/bee.lua/bee/utility/file_helper.cpp",
            "src/remotedebug/rdebug_delayload.cpp",
            "src/launcher/*.cpp",
        },
        defines = {
            "BEE_INLINE",
            "_CRT_SECURE_NO_WARNINGS",
        },
        links = {
            "ws2_32",
            "user32",
            "delayimp",
        },
        ldflags = '/DELAYLOAD:lua54.dll',
    }
end

lm:source_set 'runtime/onelua' {
    includes = {
        "3rd/bee.lua/3rd/lua/src",
    },
    sources = {
        "src/remotedebug/onelua.c",
    },
    flags = {
        platform.OS ~= "Windows" and "-fPIC"
    }
}

for _, luaver in ipairs {"lua53","lua54"} do
    lm.rootdir = '3rd/'..luaver

    if platform.OS == "Windows" then
        lm:shared_library ('runtime/'..luaver..'/'..luaver) {
            sources = {
                "*.c",
                "!lua.c",
                "!luac.c",
            },
            defines = {
                "LUAI_MAXCCALLS=1000",
                "LUA_BUILD_AS_DLL",
            }
        }
        lm:executable ('runtime/'..luaver..'/lua') {
            output = "lua",
            deps = ('runtime/'..luaver..'/'..luaver),
            sources = {
                "lua.c",
            }
        }
    else
        lm:executable ('runtime/'..luaver..'/lua') {
            sources = {
                "*.c",
                "!luac.c",
            },
            defines = {
                "LUAI_MAXCCALLS=1000",
                platform.OS == "macOS" and "LUA_USE_MACOSX",
                platform.OS == "Linux" and "LUA_USE_LINUX",
            },
            ldflags = {
                platform.OS == "Linux" and "-Wl,-E"
            },
            links = {
                "m",
                "dl",
                "readline",
            },
        }
    end

    lm.rootdir = ''

    lm:shared_library ('runtime/'..luaver..'/remotedebug') {
        deps = {
            platform.OS == "Windows" and ('runtime/'..luaver..'/'..luaver),
            "runtime/onelua",
        },
        defines = {
            "BEE_STATIC",
            "BEE_INLINE",
            platform.OS == "Windows" and "_CRT_SECURE_NO_WARNINGS",
        },
        includes = {
            "3rd/bee.lua/",
            "3rd/bee.lua/3rd/lua-seri",
            platform.OS ~= "Windows" and "3rd/"..luaver,
        },
        sources = {
            "src/remotedebug/*.cpp",
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/net/*.cpp",
            "3rd/bee.lua/bee/utility/path_helper.cpp",
            "3rd/bee.lua/bee/utility/file_helper.cpp",
            platform.OS ~= "Windows" and "!src/remotedebug/bee/rdebug_unicode.cpp",
            platform.OS == "Windows" and "3rd/bee.lua/bee/platform/version_win.cpp",
            platform.OS == "Windows" and "3rd/bee.lua/bee/utility/module_version_win.cpp",
            platform.OS == "Windows" and "3rd/bee.lua/bee/utility/unicode_win.cpp",
        },
        links = {
            platform.OS == "Windows" and "version",
            platform.OS == "Windows" and "ws2_32",
            platform.OS == "Windows" and "user32",
            platform.OS == "Windows" and "delayimp",
            platform.OS == "Linux" and "stdc++",
            platform.OS == "Linux" and "pthread",
        },
        ldflags = {
            platform.OS == "Windows" and ("/DELAYLOAD:%s.dll"):format(luaver),
        }
    }

end

lm:build 'install' {
    '$luamake', 'lua', 'make/install-runtime.lua', lm.plat, lm.arch,
    deps = {
        "runtime/lua53/lua",
        "runtime/lua54/lua",
        "runtime/lua53/remotedebug",
        "runtime/lua54/remotedebug",
        BUILD_BIN and "bee",
        BUILD_BIN and "lua",
        BUILD_BIN and "bootstrap",
        BUILD_BIN and "inject",
        BUILD_BIN and platform.OS == "Windows" and "lua54",
        platform.OS == "Windows" and "launcher",
        platform.OS == "Windows" and "runtime/lua53/lua53",
        platform.OS == "Windows" and "runtime/lua54/lua54",
    }
}

lm:default {
    "install",
}
