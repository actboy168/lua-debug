local path, pid = ...
local rt = "/runtime"
if string.packsize "T" == 8 then
    rt = rt .. "/win64"
else
    rt = rt .. "/win32"
end
if _VERSION == "Lua 5.4" then
    rt = rt .. "/lua54"
else
    rt = rt .. "/lua53"
end
local unixpath = ("pipe:%s/runtime/tmp/pid_%s.tmp"):format(path, pid)
local rdebug = assert(package.loadlib(path..rt.."/remotedebug.dll","luaopen_remotedebug"))()
local dbg = assert(loadfile(path.."/script/start_debug.lua"))(rdebug,path,'/script/?.lua',rt.."/?.dll")
dbg:io(unixpath)
dbg:wait()
dbg:start()
--TODO: support internalModule
package.loaded.debugger = dbg
