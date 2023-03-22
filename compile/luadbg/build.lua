local lm = require "luamake"

local newname <const> = "luadbg"
local luapath <const> = [[3rd/bee.lua/3rd/lua/]]
local outpath <const> = [[src/luadebug/luadbg/inc/]]
local filelist <const> = {
    "lua.h",
    "luaconf.h",
    "lualib.h",
    "lauxlib.h",
    "lua.hpp",
}

local function compile(v)
    return v
        :gsub("lua", newname)
        :gsub("lauxlib", newname.."auxlib")
end

local input = {}
local output = {}
for i, file in ipairs(filelist) do
    input[i] = luapath..file
    output[i] = outpath..compile(file)
end

output[#output + 1] = outpath..compile("luaexports.h")
output[#output + 1] = outpath..compile("luarename.h")

lm:runlua "compile_to_luadbg" {
    script = "compile/luadbg/compile.lua",
    args = {
        newname,
        luapath,
        outpath,
    },
    input = input,
    output = output,
}

lm:phony {
    input = outpath..compile("lua.hpp"),
    output = {
        "src/luadebug/rdebug_hookmgr.cpp",
        "src/luadebug/rdebug_debughost.cpp",
        "src/luadebug/rdebug_stdio.cpp",
        "src/luadebug/rdebug_utility.cpp",
        "src/luadebug/rdebug_visitor.cpp",
        "src/luadebug/util/refvalue.cpp",
    },
}

lm:phony {
    input = {
        outpath..compile("lua.hpp"),
        outpath..compile("luaexports.h"),
        outpath..compile("luarename.h"),
    },
    output = {
        "src/luadebug/luadbg/bee_module.cpp",
    },
}

lm:phony {
    input = {
        outpath..compile("luaexports.h"),
    },
    output = {
        "src/luadebug/luadbg/onelua.c",
    },
}
