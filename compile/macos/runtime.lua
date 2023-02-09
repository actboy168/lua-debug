local lm = require 'luamake'

require "compile.common.runtime"

if not lm.no_inject then
    require 'compile.common.dobby'
    require 'compile.macos.shellcode'

    lm:executable('process_inject_helper') {
        bindir = "publish/bin/",
        includes = {
            "src/process_inject",
            "3rd/bee.lua",
			"3rd/bee.lua/bee/nonstd",
        },
        sources = {
            "src/process_inject/macos/main.cc",
            "src/process_inject/macos/process_helper.mm",
            "src/process_inject/injectdll_macos.cpp",
			"3rd/bee.lua/bee/nonstd/fmt/*.cc"
        },
    }


    lm:lua_library('launcher') {
        bindir = "publish/bin/",
        export_luaopen = "off",
        deps = "dobby",
        includes = {
            "3rd/bee.lua",
            "3rd/dobby/include",
        },
        sources = {
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/utility/path_helper.cpp",
            "3rd/bee.lua/bee/utility/file_handle.cpp",
            "src/launcher/*.cpp",
            "src/launcher/hook/*.cpp",
            "src/launcher/symbol_resolver/*.cpp",
        },
        defines = {
            "BEE_INLINE",
        },
    }
end
