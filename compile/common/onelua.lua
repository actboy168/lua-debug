local lm = require "luamake"

lm:source_set 'runtime/onelua' {
    includes = {
        "3rd/bee.lua/3rd/lua",
    },
    sources = {
        "src/remotedebug/onelua.c",
    },
    linux = {
        flags = "-fPIC"
    }
}
