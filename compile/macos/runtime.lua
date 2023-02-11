local lm = require 'luamake'

require "compile.common.runtime"

if not lm.no_inject then
    require 'compile.macos.shellcode'
    lm:source_set "std_fmt"{
        includes = "3rd/bee.lua/bee/nonstd",
        sources = "3rd/bee.lua/bee/nonstd/fmt/*.cc"
    }

    lm:executable('process_inject_helper') {
        bindir = "publish/bin/macos",
        deps = "std_fmt",
        includes = {
            "src/process_inject",
            "3rd/bee.lua",
        },
        sources = {
            "src/process_inject/macos/*.cc",
            "src/process_inject/macos/process_helper.mm",
        },
    }
end
