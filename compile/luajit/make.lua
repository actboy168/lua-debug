local lm = require "luamake"

local bindir = "publish/runtime/" .. lm.runtime_platform

--the generated file must store into different directory
local defined = require "compile.luajit.defined"

local arch = defined.arch
local LUAJIT_ENABLE_LUA52COMPAT = defined.LUAJIT_ENABLE_LUA52COMPAT
local LUAJIT_NUMMODE = defined.LUAJIT_NUMMODE
local luajitDir = defined.luajitDir

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
        lm.bindir .. "/buildvm",
        string.format(" -m %s -o ", value), "$out",
        output = lm.bindir .. "/" .. fileName,
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
    lm.bindir .. "/buildvm",
    "-m",
    LJVM_MODE,
    "-o", "$out",
    output = lm.bindir .. "/lj_vm.S",
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
    lm.bindir .. "/buildvm",
    " -m folddef -o ", "$out", "$in",
    output = lm.bindir .. "/lj_folddef.h",
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
    "-D" .. LUAJIT_ENABLE_LUA52COMPAT,
    "-D" .. LUAJIT_NUMMODE,
    "-fno-stack-protector",
    "-D" .. LUAJIT_UNWIND_EXTERNAL,
    lm.os ~= "linux" and "-target " .. lm.target,
    "-c -o", "$out", "$in",
    output = lm.bindir .. "/lj_vm.obj",
    input = lm.bindir .. "/lj_vm.S",
}

local lj_str_hash_flags = {
    "-fno-stack-protector",
    U_FORTIFY_SOURCE,
    "-fPIC",
}

if arch == "x64" then
    table.insert(lj_str_hash_flags, "-msse4.2")
end

lm:source_set("lj_str_hash.c") {
    rootdir = luajitDir,
    sources = { "lj_str_hash.c" },
    includes = { '.' },
    defines = {
        LUAJIT_UNWIND_EXTERNAL,
        _FILE_OFFSET_BITS,
        _LARGEFILE_SOURCE,
        LUA_MULTILIB,
        LUAJIT_ENABLE_LUA52COMPAT,
        LUAJIT_NUMMODE
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
    deps = "lj_str_hash.c",
    sources = {
        "luajit.c",
        "lj_*.c",
        "lib_*.c",
        "!lj_str_hash.c",
        "../../../../" .. lm.bindir .. "/lj_vm.obj",
    },
    includes = {
        ".",
        lm.bindir
    },
    links = {
        "m",
        "dl",
    },
    defines = {
        LUAJIT_UNWIND_EXTERNAL,
        _FILE_OFFSET_BITS,
        _LARGEFILE_SOURCE,
        LUA_MULTILIB,
        LUAJIT_ENABLE_LUA52COMPAT,
        LUAJIT_NUMMODE
    },
    flags = {
        "-fno-stack-protector",
        U_FORTIFY_SOURCE,
        "-fPIC"
    },
}
