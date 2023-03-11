local lm = require "luamake"


lm:shared_library "shellcode_dll" {
    sources = "src/process_inject/windows/shellcode.cpp"
}

lm:executable "shellcode_compiler" {
    deps = "shellcode_dll",
    sources = "src/process_inject/windows/shellcode_compiler.cpp"
}

lm:build "shellcode" {
    deps = "shellcode_compiler",
    lm.bindir.."/shellcode_compiler.exe",
    lm.bindir.."/shellcode_dll.dll",
}
