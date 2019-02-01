local lm = require "luamake"

lm:import 'third_party/bee.lua/make.lua'

lm:lua_library "inject" {
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
    deps = "bee",
}
