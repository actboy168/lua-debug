local lm = require "luamake"

if lm.platform == "darwin-arm64" then
    lm.target = "arm64-apple-macos11"
else
    lm.target = "x86_64-apple-macos10.12"
end

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm.EXE_DIR = "publish/bin/"
lm.EXE_NAME = "lua-debug"
lm:import "3rd/bee.lua/make.lua"

lm.runtime_platform = lm.platform
require "compile.macos.runtime"
require "compile.macos.shellcode"

if lm.platform == "darwin-arm64" then
    lm:rule "luamake" {
        "$luamake",
        "-C", lm.workdir,
        "-f", "compile/macos/runtime.lua",
        "-builddir", "build/darwin-x64/"..lm.mode,
        "-mode", lm.mode,
        "-target", "x86_64-apple-macos10.12",
        "-runtime_platform", "darwin-x64",
        pool = "console",

    }
    lm:build "x86_64" {
        rule = "luamake",
    }
end


lm:executable 'process_inject_helper' {
    bindir = "publish/bin/",
    deps = { "std_fmt", "shellcode_export" },
    includes = {
        "3rd/bee.lua",
    },
    sources = {
        "src/process_inject/macos/*.cpp",
        "src/process_inject/macos/process_helper.mm",
    },
}

if lm.platform == "darwin-arm64" then
    lm:build "launcher" {
        deps = { "x86_64", "liblauncher" },
        "lipo",
        "-create",
        "-output",
        "publish/bin/launcher.so",
        "build/darwin-x64/"..lm.mode.."/bin/liblauncher.so",
        lm.bindir .. "/liblauncher.so"
    }
else
    lm:build "launcher" {
        deps = "liblauncher",
        "cp",
        lm.bindir.."/liblauncher.so",
        "publish/bin/"
    }
end

lm:default {
    "common",
    "lua-debug",
    "runtime",
    "launcher",
    "process_inject_helper",
    "launcher",
    lm.platform == "darwin-arm64" and "x86_64"
}
