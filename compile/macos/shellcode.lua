local lm = require "luamake"

lm:build "shellcode_compile" {
    "clang", "$in", "-shared", "-fpic", "-o", "$out", "-std=c++17", "-O3", lm.target and "--target="..lm.target,
    input = "src/process_inject/macos/shellcode.cpp",
    output = "$builddir/shellcode/shellcode.so",
}

lm:build "shellcode_dump" {
    "objdump", "-s", "$in", ">", "$out",
    input = "$builddir/shellcode/shellcode.so",
    output = "$builddir/shellcode/shellcode.txt",
}

lm:runlua "shellcode_export" {
    script = "compile/macos/export_shellcode.lua",
    args = { "$in", "$out" },
    input = "$builddir/shellcode/shellcode.txt",
    output = "src/process_inject/macos/shellcode.inl",
}

lm:phony "shellcode" {
    input = "src/process_inject/macos/shellcode.inl",
    output = "src/process_inject/macos/injectdll.cpp",
}