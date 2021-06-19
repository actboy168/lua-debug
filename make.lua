local lm = require "luamake"
local platform = require "bee.platform"
local fs = require "bee.filesystem"

lm.windows = {
    defines = "_WIN32_WINNT=0x0601"
}

if lm.os == "windows" then
    assert(lm.arch == "x86" or lm.arch == "x86_64")
elseif lm.os == "linux" then
    lm.arch = "x86_64"
elseif lm.os == "macos" then
    if not lm.arch then
        local function shell(command)
            local f = assert(io.popen(command, 'r'))
            local r = f:read '*l'
            f:close()
            return r:lower()
        end
        lm.arch = shell "uname -m"
    end
    assert(lm.arch == "arm64" or lm.arch == "x86_64")
end

lm:import("3rd/bee.lua/make.lua", {
    EXE_RESOURCE = "../../make/lua-debug.rc"
})

lm.builddir = ("build/%s/%s/%s"):format(lm.os, lm.arch, lm.mode)
local bindir = fs.path(lm.builddir) / 'bin'

if platform.OS == "Windows" then
    lm:source_set 'detours' {
        rootdir = "3rd/detours/src",
        sources = {
            "*.cpp",
            "!uimports.cpp"
        }
    }

    lm:lua_library 'launcher' {
        export_luaopen = "off",
        deps = {
            "detours",
        },
        includes = {
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua",
            "3rd/detours/src",
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
            "LUA_DLL_VERSION=lua54"
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
        "3rd/bee.lua/3rd/lua",
    },
    sources = {
        "src/remotedebug/onelua.c",
    },
    linux = {
        flags = "-fPIC"
    }
}

local runtimes = {}

for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
    runtimes[#runtimes+1] = "runtime/"..luaver.."/lua"
    runtimes[#runtimes+1] = "runtime/"..luaver.."/remotedebug"
    if platform.OS == "Windows" then
        runtimes[#runtimes+1] = "runtime/"..luaver.."/"..luaver
    end

    lm.rootdir = '3rd/lua/'..luaver

    if platform.OS == "Windows" then
        lm:shared_library ('runtime/'..luaver..'/'..luaver) {
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
            }
        }
        lm:executable ('runtime/'..luaver..'/lua') {
            output = "lua",
            deps = ('runtime/'..luaver..'/'..luaver),
            includes = {
                '..',
            },
            sources = {
                "lua.c",
                "../../../make/lua-debug.rc",
            },
            defines = {
                luaver == "lua51" and "_CRT_SECURE_NO_WARNINGS",
                luaver == "lua52" and "_CRT_SECURE_NO_WARNINGS",
            }
        }
    else
        lm:executable ('runtime/'..luaver..'/lua') {
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
            },
            visibility = "default",
            links = {
                "m",
                "dl",
                (luaver == "lua51" or luaver == "lua52" or luaver == "lua53") and "readline"
            },
            linux = {
                defines = "LUA_USE_LINUX",
                links = "pthread",
                ldflags = "-Wl,-E",
            },
            macos = {
                defines = {
                    "LUA_USE_MACOSX",
                    luaver == "lua51" and "LUA_USE_DLOPEN",
                }
            }
        }
    end

    lm.rootdir = ''

    local lua_version_num
    if luaver == "lua-latest" then
        lua_version_num = 504
    else
        lua_version_num = 100 * math.tointeger(luaver:sub(4,4)) + math.tointeger(luaver:sub(5,5))
    end

    lm:shared_library ('runtime/'..luaver..'/remotedebug') {
        deps = "runtime/onelua",
        defines = {
            "BEE_STATIC",
            "BEE_INLINE",
            ("DBG_LUA_VERSION=%d"):format(lua_version_num),
        },
        includes = {
            "3rd/bee.lua/",
            "3rd/bee.lua/3rd/lua-seri",
            "3rd/bee.lua/bee/nonstd",
        },
        sources = {
            "src/remotedebug/*.cpp",
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/net/*.cpp",
            "3rd/bee.lua/bee/nonstd/fmt/*.cc",
        },
        windows = {
            deps = {
                'runtime/'..luaver..'/'..luaver,
            },
			includes = '3rd/lua/'..luaver,
            defines = {
                "_CRT_SECURE_NO_WARNINGS",
                "_WIN32_WINNT=0x0601",
                ("LUA_DLL_VERSION="..luaver)
            },
            sources = {
                "3rd/bee.lua/bee/platform/version_win.cpp",
                "3rd/bee.lua/bee/utility/module_version_win.cpp",
                "3rd/bee.lua/bee/utility/unicode_win.cpp",
            },
            links = {
                "version",
                "ws2_32",
                "user32",
                "delayimp",
            },
            ldflags = {
                ("/DELAYLOAD:%s.dll"):format(luaver),
            },
        },
        linux = {
            includes = "3rd/lua/"..luaver,
            links = "pthread",
            crt = "static",
        },
        macos = {
            includes = "3rd/lua/"..luaver,
        }
    }

end

lm:build 'copy_extension' {
    '$luamake', 'lua', 'make/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'make/update_version.lua',
}

if platform.OS == "Windows" and lm.arch == "x86_64" then
    lm:build 'install' {
        '$luamake', 'lua', 'make/install_runtime.lua', bindir, lm.arch,
        deps = {
            'bee',
            'bootstrap',
            'copy_extension',
            'update_version',
            "launcher",
            runtimes,
        }
    }
else
    if platform.OS == "Windows" then
        lm:shared_library 'inject' {
            deps = {
                "bee",
                "lua54"
            },
            defines = {
                "BEE_INLINE",
            },
            includes = {
                "3rd/bee.lua",
                "3rd/bee.lua/3rd/lua",
                "3rd/wow64ext/src",
            },
            sources = {
                "src/process_inject/injectdll.cpp",
                "src/process_inject/inject.cpp",
                "3rd/wow64ext/src/wow64ext.cpp",
                "3rd/bee.lua/bee/utility/unicode_win.cpp",
            },
            links = {
                "advapi32",
            }
        }
    end

    lm:build 'install' {
        '$luamake', 'lua', 'make/install_runtime.lua', bindir, lm.arch,
        deps = {
            'copy_extension',
            'update_version',
            "bee",
            "lua",
            "bootstrap",
            platform.OS == "Windows" and "inject",
            platform.OS == "Windows" and "lua54",
            platform.OS == "Windows" and "launcher",
            runtimes,
        }
    }
end

lm:default {
    "install",
}
