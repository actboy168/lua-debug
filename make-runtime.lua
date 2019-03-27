local lm = require "luamake"

lm.arch = ARGUMENTS.arch or 'x86'
lm.bindir = "build/"..lm.plat.."/bin/runtime/"..lm.arch
lm.objdir = "build/"..lm.plat.."/obj/runtime/"..lm.arch

lm.rootdir = 'third_party/lua53'
lm:shared_library 'lua53' {
    sources = {
        "*.c",
        "!lua.c",
        "!luac.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
        "LUAI_MAXCCALLS=200"
    }
}
lm:executable 'exe-lua53' {
    deps = "lua53",
    sources = {
        "lua.c",
    }
}

lm.rootdir = 'third_party/lua54'
lm:shared_library 'lua54' {
    sources = {
        "*.c",
        "!lua.c",
        "!luac.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
        "LUAI_MAXCCALLS=200"
    }
}
lm:executable 'exe-lua54' {
    deps = "lua54",
    sources = {
        "lua.c",
    }
}

lm.rootdir = ''

lm:shared_library 'debugger' {
    deps = {
        "lua53",
    },
    defines = {
        "BEE_INLINE",
        "DEBUGGER_EXPORTS",
        "DEBUGGER_ENABLE_LAUNCH",
        "DEBUGGER_BRIDGE",
        "RAPIDJSON_HAS_STDSTRING",
        "_WINSOCK_DEPRECATED_NO_WARNINGS",
        "_WIN32_WINNT=_WIN32_WINNT_WIN7",
        "_CRT_SECURE_NO_WARNINGS",
    },
    includes = {
        "include",
        "third_party/bee.lua",
        "third_party",
        "third_party/readerwriterqueue"
    },
    sources = {
        "third_party/bee.lua/bee/net/*.cpp",
        "third_party/bee.lua/bee/error.cpp",
        "third_party/bee.lua/bee/utility/unicode_win.cpp",
        "third_party/bee.lua/bee/utility/module_version_win.cpp",
        "third_party/bee.lua/bee/platform/version_win.cpp",
        "third_party/bee.lua/bee/error/category_win.cpp",
        "include/debugger/thunk/thunk.cpp",
        "src/debugger/*.cpp",
        "!src/debugger/client/*.cpp",
        "!src/debugger/inject/*.cpp",
    },
    links = {
        "version",
        "ws2_32",
        "user32",
    }
}

lm:source_set 'detours' {
    rootdir = "third_party/detours/src",
    permissive = true,
    sources = {
        "*.cpp",
        "!uimports.cpp"
    }
}

lm:shared_library 'debugger-inject' {
    deps = {
        "lua53",
        "debugger",
        "detours"
    },
    defines = {
        "BEE_INLINE",
    },
    includes = {
        "include",
        "third_party/bee.lua",
    },
    sources = {
        "include/base/hook/inline.cpp",
        "third_party/bee.lua/bee/error.cpp",
        "third_party/bee.lua/bee/utility/unicode_win.cpp",
        "third_party/bee.lua/bee/utility/path_helper.cpp",
        "third_party/bee.lua/bee/error/category_win.cpp",
        "src/debugger/client/get_unix_path.cpp",
        "src/debugger/inject/*.cpp",
    },
    links = {
        "ws2_32",
    }
}

lm:build 'install' {
    '$luamake', 'lua', ('make/%s-install-runtime.lua'):format(lm.plat),
    deps = {
        "debugger",
        "debugger-inject",
        "exe-lua54",
        "exe-lua53",
        "lua54",
        "lua53",
    }
}

lm:default {
    "install",
}
