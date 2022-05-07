local lm = require "luamake"

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

lm.runtime_platform = lm.platform
require "compile.common.runtime"

lm:default {
    "common",
    "lua-debug",
    "runtime",
}
