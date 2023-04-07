local path, pid, debugger_path = ...
if _VERSION == nil
    or type == nil
    or assert == nil
    or tostring == nil
    or error == nil
    or dofile == nil
    or io == nil
    or os == nil
    or debug == nil
    or package == nil
    or string == nil
then
    return "wait initialized"
end

local is_luajit = tostring(assert):match('builtin') ~= nil
if is_luajit and jit == nil then
    return "wait initialized"
end

if debugger_path == "" then
    debugger_path = nil
end

local function dofile(filename, ...)
    local load = _VERSION == "Lua 5.1" and loadstring or load
    local f = assert(io.open(filename))
    local str = f:read "*a"
    f:close()
    return assert(load(str, "=(debugger.lua)"))(...)
end

local dbg = dofile(path.."/script/debugger.lua", path)
dbg:start {
    address = ("@%s/tmp/pid_%s"):format(path, pid),
    debugger_path = debugger_path,
}
dbg:event "wait"
return "ok"
