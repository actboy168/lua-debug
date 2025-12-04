local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.launcher"

lm:lua_dll 'launcher' {
    luaversion = "lua55",
    bindir = "publish/bin/",
    export_luaopen = "off",
    deps = {
        "launcher_source",
    },
}
