local lm = require "luamake"
local platform = require "bee.platform"

lm:import '3rd/bee.lua/make.lua'

if platform.OS == "Windows" then
    lm:shared_library 'inject' {
        deps = {
            "bee",
            "lua54"
        },
        includes = {
            "include",
            "3rd/bee.lua",
            "3rd/wow64ext/src",
        },
        sources = {
            "include/base/hook/injectdll.cpp",
            "include/base/hook/replacedll.cpp",
            "include/base/win/query_process.cpp",
            "src/process_inject/inject.cpp",
            "3rd/wow64ext/src/wow64ext.cpp",
        },
        links = "advapi32"
    }

    for _, arch in ipairs {"x86","x64"} do
        lm:source_set ('detours.'..arch) {
            arch = arch,
            rootdir = "3rd/detours/src",
            permissive = true,
            sources = {
                "*.cpp",
                "!uimports.cpp"
            }
        }

        lm:shared_library ('launcher.'..arch) {
            arch = arch,
            deps = {
                "lua54",
                "detours."..arch,
            },
            includes = {
                "include",
                "3rd/bee.lua",
                "3rd/bee.lua/3rd/lua/src",
            },
            sources = {
                "3rd/bee.lua/bee/error.cpp",
                "3rd/bee.lua/bee/error/category_win.cpp",
                "3rd/bee.lua/bee/utility/unicode_win.cpp",
                "3rd/bee.lua/bee/utility/path_helper.cpp",
                "3rd/bee.lua/bee/utility/file_helper.cpp",
                "include/base/hook/inline.cpp",
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
else
    lm:shared_library 'inject' {
        deps = {
            "bee",
        },
        includes = {
            "include",
            "3rd/bee.lua/3rd/lua/src",
        },
        sources = {
            "src/process_inject/inject_osx.cpp",
        }
    }
end

lm:build 'install' {
    '$luamake', 'lua', 'make/install-bin.lua', lm.plat,
    deps = {
        "bee",
        "lua",
        "inject",
        platform.OS == "Windows" and "launcher.x86",
        platform.OS == "Windows" and "launcher.x64",
        platform.OS == "Windows" and "lua54",
    }
}

lm:default {
    "install",
}
