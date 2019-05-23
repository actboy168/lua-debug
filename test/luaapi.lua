local api = {}
local macro = {}

local function readfile(filename)
    local f = assert(io.open(filename))
    local str = f:read "a"
    f:close()
    return str
end

local function dump(filename)
    local str = readfile(filename)
    for w in str:gmatch "#define[%s]+(luaL?_[%w_]+)" do
        macro[w] = true
    end
    for w in str:gmatch "luaL?_[%w_]+" do
        api[w] = true
    end
    for w in str:gmatch "luaopen_[%w_]+" do
        api[w] = true
    end
end

dump "3rd/bee.lua/3rd/lua/src/lua.h"
dump "3rd/bee.lua/3rd/lua/src/lualib.h"
dump "3rd/bee.lua/3rd/lua/src/lauxlib.h"

api.lua_Integer = true
api.lua_Unsigned = true
api.lua_Number = true

local t1 = {}
for w in pairs(api) do
    if not macro[w] then
        t1[#t1+1] = w
    end
end
table.sort(t1)
for _, w in ipairs(t1) do
   print(("#define %s rlua%s"):format(w, w:sub(4)))
end

print("")
print("#endif")
print("")
print("#else")
print("")
print("#include <lua.hpp>")
print("")

local t2 = {}
for w in pairs(api) do
    t2[#t2+1] = w
end
table.sort(t2)
for _, w in ipairs(t2) do
    print(("#define rlua%s %s"):format(w:sub(4), w))
end

print("")
print("#endif")
