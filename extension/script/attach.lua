local path, pid = ...
if _VERSION == nil
    or type == nil
    or assert == nil
    or error == nil
    or dofile == nil
    or io == nil
    or os == nil
    or debug == nil
    or package == nil
    or string == nil
then
    return false
end

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    return assert(load(str, "=(debugger.lua)"))(...)
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
local dbg = dofile(path.."/script/debugger.lua", path)
dbg:start {
    address = ("@%s/tmp/pid_%s"):format(path, pid),
    latest = isLatest(),
}
dbg:event "wait"
return true