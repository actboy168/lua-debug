local path = debug.getinfo(1,"S").source:sub(2)
    :match("(.+)[/\\][%w_.-]+$")
    :match("(.+)[/\\][%w_.-]+$")

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    local func = assert(load(str, "=(BOOTSTRAP)"))
    return func(...)
end
local dbg = dofile(path.."/script/debugger.lua",path)
dbg:set_wait("DBG", function(pid, ansi)
    dbg:start(("@%s/tmp/pid_%s"):format(path, pid), false, ansi)
end)
