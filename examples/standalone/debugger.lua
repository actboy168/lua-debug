local function dofile(filename)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    return assert(load(str, "=(debugger.lua)"))(filename)
end

local path = "../../publish"
return dofile(path .. "/script/debugger.lua")
