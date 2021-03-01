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

local function dofile(filename)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    return assert(load(str, "=(debugger.lua)"))()
end
local dbg = dofile(path.."/script/debugger.lua")
dbg:set_wait("DBG", function(params)
    local cfg = { root = path, ansi = true }
    local pid = params[1]
    for i = 2, #params do
        local param = params[i]
        cfg[param] = true
    end
    dbg:init(cfg)
    dbg:start(("@%s/tmp/pid_%s"):format(path, pid),false)
end)
