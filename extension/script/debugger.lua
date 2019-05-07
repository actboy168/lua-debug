local platform, path, pid, internalModule = ...

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
local rdebug = assert(package.loadlib(path..rt..'/remotedebug.'..ext,'luaopen_remotedebug'))()
local dbg = assert(loadfile(path..'/script/start_debug.lua'))(rdebug,path,'/script/?.lua',rt..'/?.'..ext)
package.loaded[internalModule or "debugger"] = dbg
dbg:start(("@%s/runtime/tmp/pid_%s.tmp"):format(path, pid))
