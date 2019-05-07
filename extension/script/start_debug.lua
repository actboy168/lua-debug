local rdebug, root, path, cpath = ...

local function start_worker(addr)
    local selfSource = debug.getinfo(1, 'S').source:sub(2)
    local bootstrap = ([=[
        package.path = %q
        package.cpath = %q
        local m = require 'start_master'
        m(package.path, package.cpath, %q, %q)
        local w = require 'backend.worker'
        w.skipfiles{%q}
        w.openupdate()
    ]=]):format(root..path, root..cpath, root..'/error.log', addr, selfSource)
    rdebug.start(bootstrap)
end

local m = {}

function m:start(addr, nowait)
    start_worker(addr)
    if not nowait then
        rdebug.probe 'wait_client'
    end
end

function m:event(name, ...)
    return rdebug.event('event_'..name, ...)
end

return m
