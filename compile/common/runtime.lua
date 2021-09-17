local platform = ...

local lm = require "luamake"
local runtimes = {}

require "compile.common.onelua"

for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
    runtimes[#runtimes+1] = platform..'/'..luaver.."/lua"
    runtimes[#runtimes+1] = platform..'/'..luaver.."/remotedebug"

    if lm.os == "windows" then
        runtimes[#runtimes+1] = platform..'/'..luaver.."/"..luaver
        lm:shared_library (platform..'/'..luaver..'/'..luaver) {
            rootdir = '3rd/lua/'..luaver,
            bindir = "publish/runtime/",
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

        lm:executable (platform..'/'..luaver..'/lua') {
            rootdir = '3rd/lua/'..luaver,
            bindir = "publish/runtime/",
            output = "lua",
            deps = platform..'/'..luaver..'/'..luaver,
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
    else
        lm:executable (platform..'/'..luaver..'/lua') {
            rootdir = '3rd/lua/'..luaver,
            bindir = "publish/runtime/",
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
            links = { "m", "dl", },
            linux = {
                defines = "LUA_USE_LINUX",
                links = "pthread",
                ldflags = "-Wl,-E",
            },
            android = {
                defines = "LUA_USE_LINUX",
            },
            macos = {
                defines = {
                    "LUA_USE_MACOSX",
                    luaver == "lua51" and "LUA_USE_DLOPEN",
                }
            }
        }
    end

    local lua_version_num
    if luaver == "lua-latest" then
        lua_version_num = 504
    else
        lua_version_num = 100 * math.tointeger(luaver:sub(4,4)) + math.tointeger(luaver:sub(5,5))
    end

    lm:shared_library (platform..'/'..luaver..'/remotedebug') {
        bindir = "publish/runtime/",
        deps = "runtime/onelua",
        defines = {
            "BEE_STATIC",
            "BEE_INLINE",
            ("DBG_LUA_VERSION=%d"):format(lua_version_num),
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
        },
        windows = {
            deps = platform..'/'..luaver..'/'..luaver,
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
                "shell32",
                "ole32",
                "delayimp",
            },
            ldflags = {
                ("/DELAYLOAD:%s.dll"):format(luaver),
            },
        },
        linux = {
            links = "pthread",
            crt = "static",
        },
        android = {
            links = "m",
        }
    }
end

lm:phony("runtime/"..platform) {
    input = runtimes
}
