local lm = require "luamake"

lm:exe "minilua" {
    defines = "_CRT_SECURE_NO_WARNINGS",
    sources = {
        "src/host/minilua.c"
    }
}

local dynasm_flags = {
    "-D", "P64",
    "-D", "JIT",
    "-D", "FFI",
    "-D", "WIN",
}

local arch = "x64"
if lm.platform == "win32-ia32" then
    arch = "x86"
end

local buildvm_arch_input = "src/vm_x64.dasc"
if arch == "x86" then
    buildvm_arch_input = "src/vm_x86.dasc"
end

lm:build "buildvm_arch" {
    deps = "minilua",
    lm.bindir .. "/minilua", "dynasm/dynasm.lua",
    dynasm_flags,
    "-o", "$out", "$in",
    input = buildvm_arch_input,
    output = "src/host/buildvm_arch.h",
}

lm:exe "buildvm" {
    deps = "buildvm_arch",
    defines = "_CRT_SECURE_NO_WARNINGS",
    includes = {
        "src"
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
    output = "src/lj_vm.obj",
}

lm:build "lj_bcdef" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "bcdef",
    "-o", "$out", "$in",
    output = "src/lj_bcdef.h",
    input = LJLIB,
}

lm:build "lj_ffdef" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "ffdef",
    "-o", "$out", "$in",
    output = "src/lj_ffdef.h",
    input = LJLIB,
}

lm:build "lj_libdef" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "libdef",
    "-o", "$out", "$in",
    output = "src/lj_libdef.h",
    input = LJLIB,
}

lm:build "lj_recdef" {
    deps = "buildvm",
    lm.bindir .. "/buildvm",
    "-m", "recdef",
    "-o", "$out", "$in",
    output = "src/lj_recdef.h",
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
    output = "src/lj_folddef.h",
    input = {
        "src/lj_opt_fold.c",
    }
}

lm:exe "luajit" {
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
    },
    sources = {
        "src/luajit.c",
        "src/lj_*.c",
        "src/lib_*.c",
        "src/lj_vm.obj",
    }
}
