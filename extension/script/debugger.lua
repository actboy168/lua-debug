local platform, path, pid, luaapi = ...

local rt = "/runtime"
if platform == "windows" then
    if string.packsize "T" == 8 then
        rt = rt .. "/win64"
    else
        rt = rt .. "/win32"
    end
else
    rt = rt .. "/" .. platform
end
if _VERSION == "Lua 5.4" then
    rt = rt .. "/lua54"
else
    rt = rt .. "/lua53"
end

local ext = platform == "windows" and "dll" or "so"
local remotedebug = path..rt..'/remotedebug.'..ext
if luaapi then
    assert(package.loadlib(remotedebug,'init'))()
end
local rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()
local dbg = assert(loadfile(path..'/script/start_debug.lua'))(rdebug,path,'/script/?.lua',rt..'/?.'..ext)
debug.getregistry()["lua-debug"] = dbg
dbg:start(("@%s/runtime/tmp/pid_%s.tmp"):format(path, pid))
