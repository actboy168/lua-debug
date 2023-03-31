local lm = require "luamake"

require "compile.common.frida"

lm:executable("signature_compiler") {
    deps = { "frida" },
    includes = { "3rd/frida_gum/gumpp", "3rd/bee.lua" },
    sources = { "src/launcher/tools/signature_compiler.cpp" },
}
