local lm = require "luamake"
local platform = lm.runtime_platform

lm.cxx = "c++17"

lm.defines = "_WIN32_WINNT=0x0601"
lm.builddir = ("build/%s/%s"):format(platform, lm.mode)

require "compile.common.runtime"
require "compile.common.launcher"

lm:msvc_copydll "copy_vcredist" {
    type = "vcrt",
    output = 'publish/vcredist/'..platform
}

local ArchAlias = {
    ["win32-x64"] = "x64",
    ["win32-ia32"] = "x86",
}

lm:lua_library ('launcher.'..ArchAlias[platform]) {
    bindir = "publish/bin",
    export_luaopen = "off",
    deps = {
        "launcher_source",
    }
}

lm:phony "launcher" {
    deps = 'launcher.'..ArchAlias[platform]
}
