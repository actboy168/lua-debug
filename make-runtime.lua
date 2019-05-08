local lm = require "luamake"
local platform = require "bee.platform"

local luaver = ARGUMENTS.luaver or 'lua53'
lm.arch = ARGUMENTS.arch or 'x64'
lm.bindir = ("build/%s/bin/runtime/%s/%s"):format(lm.plat, lm.arch, luaver)
lm.objdir = ("build/%s/obj/runtime/%s/%s"):format(lm.plat, lm.arch, luaver)

lm.rootdir = '3rd/'..luaver

if platform.OS == "Windows" then
    lm:shared_library (luaver) {
        sources = {
            "*.c",
            "!lua.c",
            "!luac.c",
        },
        defines = {
            "LUAI_MAXCCALLS=200",
            "LUA_BUILD_AS_DLL",
        }
    }
    lm:executable 'lua' {
        deps = luaver,
        sources = {
            "lua.c",
        }
    }
else
    lm:executable 'lua' {
        sources = {
            "*.c",
            "!luac.c",
        },
        defines = {
            "LUAI_MAXCCALLS=200",
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

lm:shared_library 'remotedebug' {
    deps = {
        platform.OS == "Windows" and luaver,
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
        "3rd/bee.lua/3rd/lua-seri/*.c",
        "src/remotedebug/*.cpp",
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/net/*.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_helper.cpp",
        platform.OS ~= "Windows" and "!src/remotedebug/bee/rdebug_unicode.cpp",
        platform.OS ~= "Windows" and "!3rd/bee.lua/bee/net/unixsocket_win.cpp",
        platform.OS == "Windows" and "3rd/bee.lua/bee/error/category_win.cpp",
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

lm:build 'install' {
    '$luamake', 'lua', 'make/install-runtime.lua', lm.plat, lm.arch, luaver,
    deps = {
        "lua",
        platform.OS == "Windows" and luaver,
        "remotedebug",
    }
}

lm:default {
    "install",
}
