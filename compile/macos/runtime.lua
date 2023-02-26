local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.dobby"

lm:source_set "src_common" {
    includes = "3rd/bee.lua",
    sources = {
        "src/common/common.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp"
    }
}

lm:lua_library 'liblauncher' {
    export_luaopen = "off",
    deps = {"dobby", "src_common" },
    includes = {
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
        "3rd/dobby/include"
    },
    sources = {
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_posix.cpp",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/*.cpp",
    },
    defines = {
        "BEE_INLINE",
        "LUA_DLL_VERSION=lua54"
    }
}
