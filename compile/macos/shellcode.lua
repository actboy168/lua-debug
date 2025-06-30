local lm = require "luamake"

lm:build "shellcode_compile" {
    args = { "clang", "$in", "-shared", "-fpic", "-o", "$out", "-std=c++17", "-O3", lm.target and "--target="..lm.target },
    inputs = "src/process_inject/macos/shellcode.cpp",
    outputs = "$builddir/shellcode/shellcode.so",
}

lm:build "shellcode_dump" {
    args = { "objdump", "-s", "$in", ">", "$out" },
    inputs = "$builddir/shellcode/shellcode.so",
    outputs = "$builddir/shellcode/shellcode.txt",
}

lm:runlua "shellcode_export" {
    script = "compile/macos/export_shellcode.lua",
    args = { "$in", "$out" },
    inputs = "$builddir/shellcode/shellcode.txt",
    outputs = "src/process_inject/macos/shellcode.inl",
}

lm:phony "shellcode" {
    inputs = "src/process_inject/macos/shellcode.inl",
    outputs = "src/process_inject/macos/injectdll.cpp",
}