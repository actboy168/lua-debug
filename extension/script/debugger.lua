local platform, root, luaapi = ...

local rt = "/runtime"
if platform == "windows" then
    if string.packsize "T" == 8 then
        rt = rt .. "/win64"
    else
        rt = rt .. "/win32"
    end
else
    assert(string.packsize "T" == 8)
    rt = rt .. "/" .. platform
end
if _VERSION == "Lua 5.4" then
    rt = rt .. "/lua54"
elseif _VERSION == "Lua 5.3" then
    rt = rt .. "/lua53"
else
    error(_VERSION .. " is not supported.")
end

local ext = platform == "windows" and "dll" or "so"
local remotedebug = root..rt..'/remotedebug.'..ext
if luaapi then
    assert(package.loadlib(remotedebug,'init'))(luaapi)
end
local rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()

local dbg = {}

function dbg:start(addr, client)
    local address = ("%q, %s"):format(addr, client == true and "true" or "false")
    rdebug.start(([=[
        package.path = %q
        package.cpath = %q
        local log = require 'common.log'
        log.file = %q
        local m = require 'backend.master'
        m(%q, %q)
        local w = require 'backend.worker'
        w.openupdate()
    ]=]):format(
          root..'/script/?.lua'
        , root..rt..'/?.'..ext
        , root..'/worker.log'
        , root..'/error.log'
        , address
    ))
end

function dbg:wait()
    rdebug.probe 'wait'
end

function dbg:event(name, ...)
    return rdebug.event('event_'..name, ...)
end

debug.getregistry()["lua-debug"] = dbg
return dbg
