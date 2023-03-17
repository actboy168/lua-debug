local lm = require "luamake"

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/"
lm:import "3rd/bee.lua/make.lua"

--TODO copy -> move
lm:copy "lua-debug" {
    input = "publish/bin/bootstrap",
    output = "publish/bin/lua-debug",
    deps = "bootstrap",
}

lm.runtime_platform = lm.platform
require "compile.common.runtime"

lm:default {
    "common",
    "lua-debug",
    "runtime",
}
