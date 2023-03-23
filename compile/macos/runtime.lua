local lm = require "luamake"

require "compile.common.config"
require "compile.common.runtime"
require "compile.common.launcher"

lm:lua_library 'launcher' {
    export_luaopen = "off",
    deps = {
        "launcher_source",
    },
}
