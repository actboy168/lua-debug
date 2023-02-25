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

local launcher_root = "src/launcher/new/";

lm:source_set ("launcher_hook_luajit"){
    includes = {"3rd/lua/luajit/src", "3rd/frida_gum/gumpp"},
    sources = launcher_root.."hook/luajit_listener.cpp",
}

lm:lua_library('liblauncher') {
    export_luaopen = "off",
    deps = {
        "frida",
        "std_format",
        "launcher_hook_luajit",
    },
    includes = {
        "3rd/bee.lua",
        "3rd/frida_gum/gumpp",
        "3rd/lua/lua54",
        "3rd/launcher/new"
    },
    sources = {
        "3rd/bee.lua/bee/error.cpp",
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        launcher_root.."*.cpp",
        launcher_root.."hook/*.cpp",
        "!"..launcher_root.."hook/luajit_listener.cpp",
        launcher_root.."symbol_resolver/*.cpp",
    },
    defines = {
        "BEE_INLINE",
    },
}
