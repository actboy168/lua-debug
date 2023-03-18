local lm = require "luamake"

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm:import "3rd/bee.lua/make.lua"
require "compile.common.lua-debug"

lm.runtime_platform = lm.platform
require "compile.common.runtime"

lm:default {
    "common",
    "lua-debug",
    "runtime",
}
