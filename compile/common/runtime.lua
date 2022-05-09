local lm = require "luamake"
local runtimes = {}

lm.cxx = "c++17"

local bindir = "publish/runtime/"..lm.runtime_platform

lm:source_set 'onelua' {
    includes = "3rd/bee.lua/3rd/lua",
    sources = "src/remotedebug/onelua.c",
    linux = {
        flags = "-fPIC"
    },
    gcc = {
        flags = "-Wno-maybe-uninitialized"
    }
}

for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest",lm.luajit == "on" and "luajit" or nil} do
    runtimes[#runtimes+1] = luaver.."/lua"
    runtimes[#runtimes+1] = luaver.."/remotedebug"

    if luaver ~= "luajit" then
        if lm.os == "windows" then
            runtimes[#runtimes+1] = luaver.."/"..luaver
            lm:shared_library (luaver..'/'..luaver) {
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
                }
            }

            lm:executable (luaver..'/lua') {
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
                }
            }
        else
            lm:executable (luaver..'/lua') {
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
                    },
                }
            }
        end
    else
        if lm.os == "windows" then
            require "compile.luajit.make_windows"
        else
            require "compile.luajit.make"
        end
    end

    local lua_version_num
    if luaver == "lua-latest" then
        lua_version_num = 504
    elseif luaver == "luajit" then
        lua_version_num = 501
    else
        lua_version_num = 100 * math.tointeger(luaver:sub(4,4)) + math.tointeger(luaver:sub(5,5))
    end

    local luaSrcDir = "3rd/lua/"..luaver;
    if luaver == "luajit" then
        luaSrcDir = luaSrcDir.."/src";
    end
    lm:shared_library (luaver..'/remotedebug') {
        bindir = bindir,
        deps = "onelua",
        defines = {
            "BEE_STATIC",
            "BEE_INLINE",
            ("DBG_LUA_VERSION=%d"):format(lua_version_num),
        },
        includes = {
            luaSrcDir,
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
            deps = luaver..'/'..luaver,
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

lm:phony "runtime" {
    input = runtimes
}
