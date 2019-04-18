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
        lm:shared_library ('launcher.'..arch) {
            arch = arch,
            includes = {
                "3rd/bee.lua",
            },
            sources = {
                "3rd/bee.lua/bee/error.cpp",
                "3rd/bee.lua/bee/error/category_win.cpp",
                "3rd/bee.lua/bee/utility/unicode_win.cpp",
                "3rd/bee.lua/bee/utility/path_helper.cpp",
                "3rd/bee.lua/bee/utility/file_helper.cpp",
                "src/launcher/*.cpp",
            },
            defines = {
                "BEE_INLINE",
            },
            links = {
                "ws2_32",
            }
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
