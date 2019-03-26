local lm = require "luamake"
local platform = require "bee.platform"

lm:import 'third_party/bee.lua/make.lua'

if platform.OS == "Windows" then
    lm:shared_library 'inject' {
        deps = {
            "bee",
            "lua54"
        },
        includes = {
            "include",
            "third_party/bee.lua",
            "third_party/wow64ext/src",
        },
        sources = {
            "include/base/hook/injectdll.cpp",
            "include/base/hook/replacedll.cpp",
            "include/base/win/query_process.cpp",
            "src/process_inject/inject.cpp",
            "third_party/wow64ext/src/wow64ext.cpp",
        },
        links = "advapi32"
    }
else
    lm:shared_library 'inject' {
        deps = {
            "bee",
        },
        includes = {
            "include",
            "third_party/bee.lua/3rd/lua/src",
        },
        sources = {
            "src/process_inject/inject_osx.cpp",
        }
    }
end

lm:build 'install' {
    '$luamake', 'lua', ('make/install-%s.lua'):format(lm.plat),
    deps = {
        "bee",
        "lua",
        "inject",
        platform.OS == "Windows" and "lua54",
    }
}

lm:default {
    "install",
}
