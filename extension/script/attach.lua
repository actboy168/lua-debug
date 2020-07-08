local path, pid, luaapi = ...

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    local func = assert(load(str, "=(BOOTSTRAP)"))
    return func(...)
end
local dbg = dofile(path.."/script/debugger.lua",path,luaapi)
dbg:start(("@%s/tmp/pid_%s"):format(path, pid))
dbg:wait()
