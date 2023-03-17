local newname, luapath, outpath = ...

local NEWNAME <const> = newname:upper()

local function lua_exports()
    local exports = {}
    local marcos = {}
    local function search_marco(line)
        local marco = line:match "^%s*#define%s+([lL][uU][aA][%w_]+)"
        if marco and not marco:match "_h$" and not marco:match "^LUA_USE_" then
            marcos[#marcos + 1] = marco
        end
    end
    for line in io.lines(luapath.."lua.h") do
        local api = line:match "^%s*LUA_API[%w%s%*_]+%(([%w_]+)%)"
        if api then
            exports[#exports + 1] = api
        else
            search_marco(line)
        end
    end
    for line in io.lines(luapath.."lauxlib.h") do
        local api = line:match "^%s*LUALIB_API[%w%s%*_]+%(([%w_]+)%)"
        if api then
            exports[#exports + 1] = api
        else
            search_marco(line)
        end
    end
    for line in io.lines(luapath.."lualib.h") do
        local api = line:match "^%s*LUALIB_API[%w%s%*_]+%(([%w_]+)%)"
        if api then
            exports[#exports + 1] = api
        else
            search_marco(line)
        end
    end
    for line in io.lines(luapath.."luaconf.h") do
        search_marco(line)
    end
    table.sort(exports)
    return exports, marcos
end

local function compile(v)
    return v
        :gsub("lua", newname)
        :gsub("LUA", NEWNAME)
        :gsub("l_", newname.."_")
        :gsub("CallInfo", newname.."CallInfo")
        :gsub("lauxlib", newname.."auxlib")
end

local function compile_to_luadbg(file)
    local compiled_file = compile(file)
    local f <const> = assert(io.open(outpath..compiled_file, "wb"))
    f:write "/* clang-format off */\n"
    for line in io.lines(luapath..file) do
        line = compile(line)
        f:write(line)
        f:write "\n"
    end
end

local exports, marcos = lua_exports()
do
    local f <const> = assert(io.open(outpath..newname.."exports.h", "wb"))
    f:write "/* clang-format off */\n"
    f:write "#pragma once\n"
    f:write "\n"
    for _, export in ipairs(exports) do
        f:write(("#define %s %s\n"):format(export, compile(export)))
    end
end
do
    local f <const> = assert(io.open(outpath..newname.."rename.h", "wb"))
    local function write(v)
        f:write(("#define %s %s\n"):format(v, compile(v)))
    end
    f:write "/* clang-format off */\n"
    f:write "#pragma once\n"
    f:write "\n"
    write "lua_State"
    write "lua_Integer"
    write "lua_Number"
    write "lua_CFunction"
    write "luaL_Stream"
    write "luaL_Buffer"
    write "luaL_Reg"
    f:write "\n"
    for _, marco in ipairs(marcos) do
        f:write(("#define %s %s\n"):format(marco, compile(marco)))
    end
end

compile_to_luadbg("lua.h")
compile_to_luadbg("luaconf.h")
compile_to_luadbg("lualib.h")
compile_to_luadbg("lauxlib.h")
compile_to_luadbg("lua.hpp")
