local lm = require "luamake"

--the generated file must store into different directory
local defined = require "compile.luajit.defined"

local arch = defined.arch
local LUAJIT_ENABLE_LUA52COMPAT = defined.LUAJIT_ENABLE_LUA52COMPAT
local LUAJIT_NUMMODE = defined.LUAJIT_NUMMODE
local luajitDir = defined.luajitDir

local LUAJIT_TARGET = string.format("LUAJIT_TARGET=LUAJIT_ARCH_%s", string.upper(arch))
local LJ_ARCH_HASFPU = "LJ_ARCH_HASFPU=1"
local LJ_ABI_SOFTFP = "LJ_ABI_SOFTFP=0"

lm:executable("minilua") {
    rootdir = luajitDir,
    defines = {
        LUAJIT_TARGET,
        LJ_ARCH_HASFPU,
        LJ_ABI_SOFTFP,
        LUAJIT_ENABLE_LUA52COMPAT,
        LUAJIT_NUMMODE
    },
    sources = { "host/minilua.c" },
    links = { "m" }
}

local arch_flags = {
    "-D", "ENDIAN_LE",
    "-D", "P64",
    "-D", "JIT",
    "-D", "FFI",
    "-D", "FPU",
    "-D", "HFABI",
    "-D", "VER=80",
    "-D", "DUALNUM"
}

lm:build("builvm_arch.h") {
    deps = "minilua",
    args = {
        lm.bindir .. "/minilua",
        luajitDir .. "/../dynasm/dynasm.lua",
        arch_flags,
        "-o", "$out", "$in",
    },
    inputs = luajitDir .. string.format("/vm_%s.dasc", arch),
    outputs = lm.bindir .. "/buildvm_arch.h"
}

lm:executable("buildvm") {
    rootdir = luajitDir,
    deps = "builvm_arch.h",
    objdeps = { lm.bindir .. "/buildvm_arch.h" },
    defines = {
        LUAJIT_TARGET,
        LJ_ARCH_HASFPU,
        LJ_ABI_SOFTFP,
        LUAJIT_ENABLE_LUA52COMPAT,
        LUAJIT_NUMMODE
    },
    sources = {
        "host/*.c",
        "!host/minilua.c"
    },
    includes = {
        ".",
        "../../../../" .. lm.bindir
    }
}
