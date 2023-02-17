local lm = require 'luamake'

lm:source_set "std_format" {
    includes = "3rd/bee.lua/bee/nonstd/3rd/fmt/include",
    sources = {
        "3rd/bee.lua/bee/nonstd/3rd/fmt/src/format.cc",
        "3rd/bee.lua/bee/nonstd/3rd/fmt/src/os.cc",
    }
}
