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
require "compile.common.runtime"
require "compile.common.dobby"
require "compile.macos.shellcode"

if lm.platform == "darwin-arm64" then
    lm:rule "luamake" {
        "$luamake",
        "-C", lm.workdir,
        "-f", "compile/common/runtime.lua",
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

lm:source_set "std_fmt" {
    includes = "3rd/bee.lua/bee/nonstd",
    sources = "3rd/bee.lua/bee/nonstd/fmt/*.cc"
}

lm:executable 'process_inject_helper' {
    bindir = "publish/bin/",
    deps = { "std_fmt", "shellcode" },
    includes = {
        "src/process_inject",
        "3rd/bee.lua",
    },
    sources = {
        "src/process_inject/macos/*.cpp",
        "src/process_inject/macos/process_helper.mm",
    },
}

lm:lua_library 'launcher' {
    bindir = "publish/bin/",
    export_luaopen = "off",
    deps = "dobby",
    includes = {
        "3rd/bee.lua",
        "3rd/bee.lua/3rd/lua",
        "3rd/dobby/include"
    },
    sources = {
        "3rd/bee.lua/bee/utility/path_helper.cpp",
        "3rd/bee.lua/bee/utility/file_handle.cpp",
        "3rd/bee.lua/bee/utility/file_handle_posix.cpp",
        "src/remotedebug/rdebug_delayload.cpp",
        "src/launcher/*.cpp",
    },
    defines = {
        "BEE_INLINE",
        "LUA_DLL_VERSION=lua54"
    }
}

lm:default {
    "common",
    "lua-debug",
    "runtime",
    "process_inject_helper",
    "launcher",
    lm.platform == "darwin-arm64" and "x86_64"
}
