local lm = require "luamake"

if lm.target_arch == nil then
    lm.builddir = ("build/%s/_/%s"):format(lm.os, lm.mode)
    lm:build "x86" {
        "$luamake",
        "-target_arch", "x86",
        pool = "console",
    }
    lm:build "x86_64" {
        "$luamake",
        "-target_arch", "x86_64",
        pool = "console",
    }
    lm:build 'copy_extension' {
        '$luamake', 'lua', 'compile/copy_extension.lua',
    }
    lm:build 'update_version' {
        '$luamake', 'lua', 'compile/update_version.lua',
    }
    lm:build 'install-x86' {
        '$luamake', 'lua', 'compile/install_runtime.lua', ("build/%s/%s/%s/bin"):format(lm.os, "x86", lm.mode), "x86",
        deps = "x86"
    }
    lm:build 'install-x86_64' {
        '$luamake', 'lua', 'compile/install_runtime.lua', ("build/%s/%s/%s/bin"):format(lm.os, "x86_64", lm.mode), "x86_64",
        deps = "x86_64"
    }
    return
end

local fs = require "bee.filesystem"

lm.arch = lm.target_arch
lm.defines = "_WIN32_WINNT=0x0601"

lm.EXE_NAME = "lua-debug"
lm.EXE_RESOURCE = "../../compile/windows/lua-debug.rc"
lm:import "3rd/bee.lua/make.lua"

lm.builddir = ("build/%s/%s/%s"):format(lm.os, lm.arch, lm.mode)
local bindir = fs.path(lm.builddir) / 'bin'

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

lm:source_set 'runtime/onelua' {
    includes = {
        "3rd/bee.lua/3rd/lua",
    },
    sources = {
        "src/remotedebug/onelua.c",
    }
}

local runtimes = {}

for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
    runtimes[#runtimes+1] = "runtime/"..luaver.."/lua"
    runtimes[#runtimes+1] = "runtime/"..luaver.."/remotedebug"
    runtimes[#runtimes+1] = "runtime/"..luaver.."/"..luaver

    lm:shared_library ('runtime/'..luaver..'/'..luaver) {
        rootdir = '3rd/lua/'..luaver,
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
        rootdir = '3rd/lua/'..luaver,
        output = "lua",
        deps = ('runtime/'..luaver..'/'..luaver),
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
        }
    }

    local lua_version_num
    if luaver == "lua-latest" then
        lua_version_num = 504
    else
        lua_version_num = 100 * math.tointeger(luaver:sub(4,4)) + math.tointeger(luaver:sub(5,5))
    end

    lm:shared_library ('runtime/'..luaver..'/remotedebug') {
        deps = {
            "runtime/onelua",
            'runtime/'..luaver..'/'..luaver,
        },
        defines = {
            "BEE_STATIC",
            "BEE_INLINE",
            ("DBG_LUA_VERSION=%d"):format(lua_version_num),
            "_CRT_SECURE_NO_WARNINGS",
            ("LUA_DLL_VERSION="..luaver)
        },
        includes = {
            "3rd/lua/"..luaver,
            "3rd/bee.lua/",
            "3rd/bee.lua/3rd/lua-seri",
            "3rd/bee.lua/bee/nonstd",
        },
        sources = {
            "src/remotedebug/*.cpp",
            "src/remotedebug/thunk/*.cpp",
            "src/remotedebug/bee/*.cpp",
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/net/*.cpp",
            "3rd/bee.lua/bee/nonstd/fmt/*.cc",
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
    }

end

if lm.arch == "x86_64" then
    lm:default {
        'lua-debug',
        "launcher",
        runtimes,
    }
else
    lm:lua_dll 'inject' {
        deps = "lua54",
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

    lm:default {
        "lua-debug",
        "inject",
        "launcher",
        runtimes,
    }
end
