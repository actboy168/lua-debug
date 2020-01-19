local platform, root, luaapi, ansi = ...

local arch = (function()
    if string.packsize then
        local size = string.packsize "T"
        if size == 8 then
            return 64
        end
        if size == 4 then
            return 32
        end
    else
        if platform ~= "windows" then
            return 64
        end
        local size = #tostring(io.stderr)
        if size <= 15 then
            return 32
        end
        return 64
    end
    assert(false, "unknown arch")
end)()

local rt = "/runtime"
if platform == "windows" then
    if arch == 64 then
        rt = rt .. "/win64"
    else
        rt = rt .. "/win32"
    end
else
    assert(arch == 64)
    rt = rt .. "/" .. platform
end
if _VERSION == "Lua 5.4" then
    rt = rt .. "/lua54"
elseif _VERSION == "Lua 5.3" then
    rt = rt .. "/lua53"
elseif _VERSION == "Lua 5.2" then
    rt = rt .. "/lua52"
elseif _VERSION == "Lua 5.1" then
    rt = rt .. "/lua51"
else
    error(_VERSION .. " is not supported.")
end

local ext = platform == "windows" and "dll" or "so"
local remotedebug = root..rt..'/remotedebug.'..ext
if luaapi then
    assert(package.loadlib(remotedebug,'init'))(luaapi)
end
local rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()

if ansi then
    root = rdebug.a2u(root)
end

local dbg = {}

function dbg:start(addr, client)
    local address = ("%q, %s"):format(addr, client == true and "true" or "false")
    local bootstrap_lua = ([[
        package.path = %q
        package.cpath = %q
        if debug.setcstacklimit then debug.setcstacklimit(1000) end
        require "remotedebug.thread".bootstrap_lua = debug.getinfo(1, "S").source
    ]]):format(
          root..'/script/?.lua'
        , root..rt..'/?.'..ext
    )
    rdebug.start(("assert(load(%q))(...)"):format(bootstrap_lua) .. ([[
        local logpath = %q
        local log = require 'common.log'
        log.file = logpath..'/worker.log'
        require 'backend.master' (logpath, %q, true)
        require 'backend.worker' .openupdate()
    ]]):format(
          root
        , ansi and rdebug.a2u(address) or address
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
