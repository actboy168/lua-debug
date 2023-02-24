local lm = require "luamake"

require "compile.common.runtime"
require "compile.common.frida"

lm:source_set "std_format" {
    includes = "3rd/bee.lua/bee/nonstd/3rd/fmt",
    sources = {
        "3rd/bee.lua/bee/nonstd/3rd/format.cc",
        "3rd/bee.lua/bee/nonstd/3rd/os.cc",
    }
}

lm:lua_library('liblauncher') {
    export_luaopen = "off",
    deps = {
        "frida",
        "std_format",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/frida_gum/gumpp",
        "3rd/lua/lua54",
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "src/launcher/*.cpp",
        "src/launcher/hook/*.cpp",
        "!src/launcher/hook/luajit.cpp",
        "src/launcher/symbol_resolver/*.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
}
