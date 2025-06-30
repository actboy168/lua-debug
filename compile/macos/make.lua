local lm = require "luamake"

if lm.platform == "darwin-arm64" then
    lm.target = "arm64-apple-macos11"
else
    lm.target = "x86_64-apple-macos10.12"
end

lm.builddir = ("build/%s/%s"):format(lm.platform, lm.mode)
lm:import "compile/common/bee.lua"
require "compile.common.lua-debug"

lm.runtime_platform = lm.platform
require "compile.macos.runtime"
require "compile.macos.shellcode"

if lm.platform == "darwin-arm64" then
    require "compile.common.run_luamake"
    lm:build "x86_64" {
        rule = "run_luamake",
        inputs = "compile/macos/runtime.lua",
        args = {
            "-builddir", "build/darwin-x64/"..lm.mode,
            "-runtime_platform", "darwin-x64",
            "-target", "x86_64-apple-macos10.12",
        },
    }
end


lm:executable 'process_inject_helper' {
    bindir = "publish/bin/",
    deps = { "shellcode" },
    includes = {
        "3rd/bee.lua",
    },
    sources = {
        "src/process_inject/macos/*.cpp",
        "src/process_inject/macos/process_helper.mm",
    },
}

if lm.platform == "darwin-arm64" then
    lm:build "merge_launcher" {
        deps = { "x86_64", "launcher" },
        args = {
            "lipo", "-create", "-output", "$out", "$in",
            "build/darwin-x64/"..lm.mode.."/bin/launcher.so", --TODO
        },
        inputs = "$bin/launcher.so",
        outputs = "publish/bin/launcher.so",
    }
else
    lm:copy "merge_launcher" {
        deps = "launcher",
        inputs = "$bin/launcher.so",
        outputs = "publish/bin/launcher.so"
    }
end

lm:default {
    "common",
    "lua-debug",
    "runtime",
    "process_inject_helper",
    "merge_launcher",
    lm.platform == "darwin-arm64" and "x86_64",
}
