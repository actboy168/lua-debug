local rdebug, root, path, cpath = ...

local function start_worker(addr)
    local bootstrap = ([=[
        package.path = %q
        package.cpath = %q
        local m = require 'start_master'
        m(package.path, package.cpath, %q, %q)
        local w = require 'backend.worker'
        w.openupdate()
    ]=]):format(root..path, root..cpath, root..'/error.log', addr)
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
