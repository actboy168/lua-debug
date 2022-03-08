local lm = require "luamake"

local luaver = "luajit"
local luajitDir = '3rd/lua/' .. luaver
local bindir = "publish/runtime/"..lm.runtime_platform

lm:exe "minilua" {
    rootdir= luajitDir,
    defines = "_CRT_SECURE_NO_WARNINGS",
    sources = {
        "src/host/minilua.c"
    }
}

local arch = lm.arch
if arch == "x86_64" then
    arch = "x64"
else
    assert(arch == "x86")
end

local dynasm_flags = {
    "-D", "JIT",
    "-D", "FFI",
    "-D", "WIN",
}
if arch =="x64" then
    table.insert(dynasm_flags,"-D")
    table.insert(dynasm_flags,"P64")
end


local buildvm_arch_input = "src/vm_x64.dasc"
if arch == "x86" then
    buildvm_arch_input = "src/vm_x86.dasc"
end

lm:build "buildvm_arch" {
    deps = "minilua",
    lm.bindir .. "/minilua", luajitDir.."/dynasm/dynasm.lua",
    dynasm_flags,
    "-o", "$out", "$in",
    input = luajitDir.."/"..buildvm_arch_input,
    output =  lm.bindir .."/buildvm_arch.h",
}

lm:exe "buildvm" {
    rootdir= luajitDir,
    deps = "buildvm_arch",
    objdeps={"buildvm_arch"},
    defines = {"_CRT_SECURE_NO_WARNINGS"},
    includes = {
        "src",
        "../../../"..lm.bindir
    },
    sources = {
        "src/host/*.c",
        "!src/host/minilua.c",
    }
}

local LJLIB = {
    "src/lib_base.c",
    "src/lib_math.c",
    "src/lib_bit.c",
    "src/lib_string.c",
    "src/lib_table.c",
    "src/lib_io.c",
    "src/lib_os.c",
    "src/lib_package.c",
    "src/lib_debug.c",
    "src/lib_jit.c",
    "src/lib_ffi.c",
    "src/lib_buffer.c",
}

lm:build "lj_peobj" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "peobj",
    "-o", "$out",
    output = lm.bindir.."/lj_vm.obj",
}

lm:build "lj_bcdef" {
    rootdir=luajitDir,
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "bcdef",
    "-o", "$out", "$in",
    output = lm.bindir.."/lj_bcdef.h",
    input = LJLIB,
}

lm:build "lj_ffdef" {
    rootdir=luajitDir,
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "ffdef",
    "-o", "$out", "$in",
    output = lm.bindir.."/lj_ffdef.h",
    input = LJLIB,
}

lm:build "lj_libdef" {
    rootdir=luajitDir,
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "libdef",
    "-o", "$out", "$in",
    output = lm.bindir.."/lj_libdef.h",
    input = LJLIB,
}

lm:build "lj_recdef" {
    rootdir=luajitDir,
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "recdef",
    "-o", "$out", "$in",
    output = lm.bindir.."/lj_recdef.h",
    input = LJLIB,
}

--lm:build "lj_vmdef" {
--    deps = "buildvm",
--    lm.bindir.."/buildvm",
--    "-m", "vmdef",
--    "-o", "$out", "$in",
--    output = "jit/vmdef.lua",
--    input = LJLIB,
--}

lm:build "lj_folddef" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "folddef",
    "-o", "$out", "$in",
    output = lm.bindir.."/lj_folddef.h",
    input = {
        luajitDir.."/src/lj_opt_fold.c",
    }
}


lm:shared_library "luajit/luajit" {
    rootdir= luajitDir,
    bindir = bindir,
    objdeps = {
        "lj_bcdef",
        "lj_ffdef",
        "lj_libdef",
        "lj_recdef",
        --"lj_vmdef",
        "lj_folddef",
    },
    defines = {
        "_CRT_SECURE_NO_WARNINGS",
        "LUA_BUILD_AS_DLL"
    },
    sources = {
        "!src/luajit.c",
        "!src/lj_init.c",
        "src/lj_*.c",
        "src/lib_*.c",
        "../../../".. lm.bindir.."/lj_vm.obj",
    },
    includes={
        ".",
        "../../../"..lm.bindir
    }
}

lm:exe "luajit/lua" {
    rootdir= luajitDir,
    bindir = bindir,
   deps="luajit/luajit",
    defines = {
        "_CRT_SECURE_NO_WARNINGS",
    },
    sources = {
        "src/luajit.c",
        "src/lj_init.c",
    },
    includes={
        ".",
        "../../../"..lm.bindir
    }
}
