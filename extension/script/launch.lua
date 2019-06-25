local platform, path, pid = ...

local function dofile(filename, ...)
    local f = assert(io.open(filename))
    local str = f:read "a"
    f:close()
    local func = assert(load(str, "=(BOOTSTRAP)"))
    return func(...)
end

local dbg = dofile(path.."/script/debugger.lua",platform,path)
dbg:start(("@%s/tmp/pid_%s"):format(path, pid))
return dbg
