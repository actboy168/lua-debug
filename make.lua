local lm = require "luamake"

lm:import 'third_party/bee.lua/make.lua'

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

lm:build 'install' {
    '$luamake', 'lua', 'make/install.lua',
    deps = {
        "bee",
        "lua54",
        "lua",
        "inject",
    }
}

lm:default {
    "install",
}
