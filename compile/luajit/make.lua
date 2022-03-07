local lm = require "luamake"

local bindir = "publish/runtime/"..lm.runtime_platform

--the generated file must store into different directory
local arch = "x64"
if lm.platform == "windows-ia32" then
    arch = "x86"
    error("never test now")
elseif lm.platform == "linux-arm64" or lm.platform == "darwin-arm64" then
    arch = "arm64"
end

local luaver = "luajit"
local LUAJIT_TARGET = string.format("LUAJIT_TARGET=LUAJIT_ARCH_%s", string.upper(arch))
local LJ_ARCH_HASFPU = "LJ_ARCH_HASFPU=1"
local LJ_ABI_SOFTFP = "LJ_ABI_SOFTFP=0"
local luajitDir = '3rd/lua/' .. luaver .. "/src"

lm:executable("minilua") {
    rootdir = luajitDir,
    defines = {
        LUAJIT_TARGET,
        LJ_ARCH_HASFPU,
        LJ_ABI_SOFTFP
    },
    sources = { "host/minilua.c" }
}

local binPath = lm.builddir.."/bin/"

local arch_flags
if arch == "arm64" then
    arch_flags = {
        "-D", "ENDIAN_LE",
        "-D", "P64",
        "-D", "JIT",
        "-D", "FFI",
        "-D", "FPU",
        "-D", "HFABI",
        "-D", "VER=80",
    }
elseif arch == "x64" then
    arch_flags = {
        "-D", "ENDIAN_LE",
        "-D", "P64",
        "-D", "JIT",
        "-D", "FFI",
        "-D", "FPU",
        "-D", "HFABI",
    }
else
    error("unsupported architecture:"..arch)
end

lm:build("builvm_arch.h") {
    deps = "minilua",
    binPath .. "minilua",
    luajitDir .. "/../dynasm/dynasm.lua",
    arch_flags,
    "-o", "$out", "$in",
    input = luajitDir .. string.format("/vm_%s.dasc", arch),
    output = binPath .. "/buildvm_arch.h"
}

lm:executable("buildvm") {
    rootdir = luajitDir,
    deps = "builvm_arch.h",
    objdeps = { binPath .. "/buildvm_arch.h"},
    defines = {
        LUAJIT_TARGET,
        LJ_ARCH_HASFPU,
        LJ_ABI_SOFTFP
    },
    sources = {
        "host/*.c",
        "!host/minilua.c"
    },
    includes = {
        ".",
        "../../../../" .. binPath
    }
}
local LJLIB_C = {
    luajitDir .. "/lib_base.c ",
    luajitDir .. "/lib_math.c ",
    luajitDir .. "/lib_bit.c ",
    luajitDir .. "/lib_string.c ",
    luajitDir .. "/lib_table.c ",
    luajitDir .. "/lib_io.c ",
    luajitDir .. "/lib_os.c ",
    luajitDir .. "/lib_package.c ",
    luajitDir .. "/lib_debug.c ",
    luajitDir .. "/lib_jit.c ",
    luajitDir .. "/lib_ffi.c ",
    luajitDir .. "/lib_buffer.c",
}

local function buildvmX(value)
    local fileName = string.format("lj_%s.h", value)
    lm:build(fileName) {
        deps = { "buildvm" },
        binPath .. "buildvm",
        string.format(" -m %s -o ", value), "$out",
        output = binPath .. fileName,
        LJLIB_C,
    }
end

local LJVM_MODE = "elfasm"
if lm.os == "windows" then
    LJVM_MODE = "peobj"
elseif lm.os == "macos" then
    LJVM_MODE = "machasm"
end

lm:build("lj_vm.S") {
    deps = "buildvm",
    binPath .. "buildvm",
    "-m",
    LJVM_MODE,
    "-o", "$out",
    output = binPath .. "lj_vm.S",
}

buildvmX("bcdef")
buildvmX("ffdef")
buildvmX("libdef")
buildvmX("recdef")

--[[
lm:build("luajit/vmdef.lua") {
    deps = "buildvm",
    binPath .. "buildvm",
    " -m vmdef -o ", "$out", "$in",
    output = binPath .. "/vmdef.lua", 
    input = LJLIB_C,
}
]]

lm:build("lj_folddef.h") {
    deps = "buildvm",
    binPath .. "buildvm",
    " -m folddef -o ", "$out", "$in",
    output = binPath .. "lj_folddef.h",
    input = luajitDir .. "/lj_opt_fold.c",
}


local _FILE_OFFSET_BITS = "_FILE_OFFSET_BITS=64"
local _LARGEFILE_SOURCE = "_LARGEFILE_SOURCE"
local U_FORTIFY_SOURCE = "-U_FORTIFY_SOURCE"
local LUAJIT_UNWIND_EXTERNAL = "LUAJIT_UNWIND_EXTERNAL"
local LUA_MULTILIB = "LUA_MULTILIB=\"lib\""

lm:build("lj_vm.obj") {
    deps = { "lj_vm.S" },
    "$cc",
    "-Wall",
    "-D" .. _FILE_OFFSET_BITS,
    "-D" .. _LARGEFILE_SOURCE,
    U_FORTIFY_SOURCE,
    "-D" .. LUA_MULTILIB,
    "-fno-stack-protector",
    "-D" .. LUAJIT_UNWIND_EXTERNAL,
    "-target",lm.target,
    "-c -o", "$out", "$in",
    output = binPath .. "lj_vm.obj",
    input = binPath .. "lj_vm.S",
}

local lj_str_hash_flags = {
    "-fno-stack-protector",
    U_FORTIFY_SOURCE,
    "-fPIC",
}

if arch == "x64" then
    table.insert(lj_str_hash_flags,"-msse4.2")
end

lm:source_set("lj_str_hash.c"){
    rootdir = luajitDir,
    sources={"lj_str_hash.c"},
    includes = {'.'},
    defines = {
        LUAJIT_UNWIND_EXTERNAL,
        _FILE_OFFSET_BITS,
        _LARGEFILE_SOURCE,
        LUA_MULTILIB
    },
    flags = lj_str_hash_flags
}

lm:executable("luajit/lua") {
    rootdir = luajitDir,
    bindir = bindir,
    objdeps = {
        "lj_vm.obj",
        "lj_folddef.h",
        "lj_bcdef.h",
        "lj_ffdef.h",
        "lj_libdef.h",
        "lj_recdef.h",
    },
    deps="lj_str_hash.c",
    sources = {
        "luajit.c",
        "lj_*.c",
        "lib_*.c",
        "!lj_str_hash.c",
        "../../../../" .. binPath .. "lj_vm.obj",
    },
    includes = {
        ".",
        "../../../../" .. binPath
    },
    links = {
        "m",
    },
    defines = {
        LUAJIT_UNWIND_EXTERNAL,
        _FILE_OFFSET_BITS,
        _LARGEFILE_SOURCE,
        LUA_MULTILIB
    },
    flags = {
        "-fno-stack-protector",
        U_FORTIFY_SOURCE,
        "-fPIC"
    },
}