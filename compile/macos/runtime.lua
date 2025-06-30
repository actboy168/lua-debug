local lm = require "luamake"

require "compile.common.config"
require "compile.common.runtime"
require "compile.common.launcher"

lm:lua_dll 'launcher' {
    export_luaopen = "off",
    deps = {
        "launcher_source",
    },
}
