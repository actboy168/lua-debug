local path = (function ()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
        :match("(.+)[/\\][%w_.-]+$")
        :match("(.+)[/\\][%w_.-]+$")
        :match("(.+)[/\\][%w_.-]+$")
        :match("(.+)[/\\][%w_.-]+$")
end)()

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    local func = assert(load(str, "=(BOOTSTRAP)"))
    return func(...)
end
local dbg = dofile(path.."/script/debugger.lua",path)
dbg:set_wait("DBG", function(pid)
    dbg:start(("@%s/tmp/pid_%s"):format(path, pid),false,true)
end)
