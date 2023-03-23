local lm = require "luamake"

lm.compile_commands = "build"
lm.c = "c11"
lm.cxx = "c++17"

lm.mode = "debug"

lm.msvc = {
    flags = "/wd5105"
}
