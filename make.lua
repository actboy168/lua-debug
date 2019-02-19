local lm = require "luamake"

lm.rootdir = 'third_party/lua53'
lm:shared_library 'dll-lua53' {
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
    deps = "dll-lua53",
    sources = {
        "lua.c",
    }
}

lm.rootdir = 'third_party/lua54'
lm:shared_library 'dll-lua54' {
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
    deps = "dll-lua54",
    sources = {
        "lua.c",
    }
}

lm:import 'third_party/bee.lua/make.lua'

lm:shared_library 'inject' {
    deps = {
        "bee",
        "lua54"
    },
    includes = {
        "include",
        "third_party/bee.lua",
        "third_party/wow64ext/src",
    },
    sources = {
        "include/base/hook/injectdll.cpp",
        "include/base/hook/replacedll.cpp",
        "include/base/win/query_process.cpp",
        "src/process_inject/inject.cpp",
        "third_party/wow64ext/src/wow64ext.cpp",
    },
    links = "advapi32"
}

lm:shared_library 'debugger' {
    deps = {
        "dll-lua53",
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
        "dll-lua53",
        "debugger",
        "detours"
    },
    includes = {
        "include",
        "third_party/bee.lua",
    },
    sources = {
        "include/base/hook/inline.cpp",
        "src/debugger/client/get_unix_path.cpp",
        "src/debugger/inject/*.cpp",
    }
}

lm:build 'install' {
    '$luamake', 'lua', 'make/install.lua',
    deps = {
        "bee",
        "lua54",
        "lua",
        "inject",
        "debugger-inject",
    }
}

lm:default {
    "install",
}
