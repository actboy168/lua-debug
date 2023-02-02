local lm = require 'luamake'

require "compile.common.runtime"

if not lm.no_inject then
    require 'compile.common.dobby' (lm.bindir)
    local ext = ".so"
    local output = "libdobby"
    if lm.os == 'macos' then
        ext = '.dylib'
    end
    output = output .. ext
    lm:build 'libdobby' {
        deps = "build_dobby",
        "cp",
        lm.bindir .. "/" .. output,
        "$out",
        output = "publish/bin/" .. output,
    }

    lm:executable('process_inject_helper') {
        bindir = "publish/bin/",
        includes = {
            "src/process_inject"
        },
        sources = {
            "src/process_inject/macos/main.cc",
            "src/process_inject/macos/process_helper.mm",
            "src/process_inject/injectdll_macos.cpp",
        },
    }


    lm:lua_library('launcher') {
        bindir = "publish/bin/",
        export_luaopen = "off",
        deps = "libdobby",
        includes = {
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua",
            "3rd/dobby/include",
        },
        sources = {
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/utility/path_helper.cpp",
            "3rd/bee.lua/bee/utility/file_handle.cpp",
            "src/launcher/*.cpp",
        },
        defines = {
            "BEE_INLINE",
            "LUA_DLL_VERSION=lua54"
        },
    }
end
