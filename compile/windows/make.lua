local lm = require "luamake"

if lm.target_arch == nil then
    lm.builddir = ("build/%s/_/%s"):format(lm.os, lm.mode)
    lm:build "x86" {
        "$luamake", "-target_arch", "x86",
        pool = "console",
    }
    lm:build "x86_64" {
        "$luamake", "-target_arch", "x86_64",
        pool = "console",
    }
    lm:build 'copy_extension' {
        '$luamake', 'lua', 'compile/copy_extension.lua',
    }
    lm:build 'update_version' {
        '$luamake', 'lua', 'compile/update_version.lua',
    }
    lm:copy 'copy_bootstrap' {
        input = "extension/script/bootstrap.lua",
        output = "publish/bin/windows/main.lua",
    }
    lm:build "copy_vcredist" {
        "$luamake", "lua", "compile/windows/copy_vcredist.lua"
    }
    return
end

lm.arch = lm.target_arch
lm.defines = "_WIN32_WINNT=0x0601"
lm.builddir = ("build/%s/%s/%s"):format(lm.os, lm.arch, lm.mode)

assert(loadfile("compile/common/runtime.lua"))(lm.os.."/"..lm.arch)

do
    lm:source_set 'detours' {
        rootdir = "3rd/detours/src",
        sources = {
            "*.cpp",
            "!uimports.cpp"
        }
    }
    local ArchAlias = {
        x86_64 = "x64",
        x86 = "x86",
    }
    lm:lua_library ('launcher.'..ArchAlias[lm.arch]) {
        bindir = "publish/bin/windows",
        export_luaopen = "off",
        deps = "detours",
        includes = {
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua",
            "3rd/detours/src",
        },
        sources = {
            "3rd/bee.lua/bee/error.cpp",
            "3rd/bee.lua/bee/utility/unicode_win.cpp",
            "3rd/bee.lua/bee/utility/path_helper.cpp",
            "3rd/bee.lua/bee/utility/file_helper.cpp",
            "src/remotedebug/rdebug_delayload.cpp",
            "src/launcher/*.cpp",
        },
        defines = {
            "BEE_INLINE",
            "_CRT_SECURE_NO_WARNINGS",
            "LUA_DLL_VERSION=lua54"
        },
        links = {
            "ws2_32",
            "user32",
            "shell32",
            "ole32",
            "delayimp",
        },
        ldflags = '/DELAYLOAD:lua54.dll',
    }
    lm:phony "launcher" {
        deps = 'launcher.'..ArchAlias[lm.arch]
    }
end

if lm.arch == "x86" then
    lm.EXE_DIR = "publish/bin/windows"
    lm.EXE_NAME = "lua-debug"
    lm.EXE_RESOURCE = "../../compile/windows/lua-debug.rc"
    lm:import "3rd/bee.lua/make.lua"
    lm:lua_dll 'inject' {
        bindir = "publish/bin/windows",
        deps = "lua54",
        defines = "BEE_INLINE",
        includes = {
            "3rd/bee.lua",
            "3rd/bee.lua/3rd/lua",
            "3rd/wow64ext/src",
        },
        sources = {
            "src/process_inject/injectdll.cpp",
            "src/process_inject/inject.cpp",
            "3rd/wow64ext/src/wow64ext.cpp",
            "3rd/bee.lua/bee/utility/unicode_win.cpp",
        },
        links = "advapi32",
    }
end

lm:default {
    lm.arch == "x86" and "lua-debug",
    lm.arch == "x86" and "inject",
    "launcher",
    "runtime/"..lm.os.."/"..lm.arch,
}
