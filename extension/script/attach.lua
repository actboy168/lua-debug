local path, pid, luaapi = ...

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    local func = assert(load(str, "=(debugger.lua)"))
    return func(...)
end
local function isLatest()
    local ipc = dofile(path.."/script/common/ipc.lua")
    local fd = ipc(path, pid, "luaVersion")
    local result = false
    if fd then
        result = "latest" == fd:read "a"
        fd:close()
    end
    return result
end
local dbg = dofile(path.."/script/debugger.lua",path,isLatest(),luaapi)
dbg:start(("@%s/tmp/pid_%s"):format(path, pid))
dbg:event "wait"
