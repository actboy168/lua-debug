local lm = require "luamake"
local platform = require "bee.platform"

lm.gcc = 'clang'
lm.gxx = 'clang++'

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
            "src/base/win/query_process.cpp",
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

lm:build 'install' {
    '$luamake', 'lua', 'make/install-bin.lua', lm.plat,
    deps = {
        "bee",
        "lua",
        "bootstrap",
        "inject",
        platform.OS == "Windows" and "lua54",
    }
}

lm:default {
    "install",
}
