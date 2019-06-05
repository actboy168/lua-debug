local rdebug, root, path, cpath = ...

local m = {}

function m:start(addr, client)
    local bootstrap = ([=[
        package.path = %q
        package.cpath = %q
        local m = require 'start_master'
        m(package.path, package.cpath, %q, %q, %q)
        local w = require 'backend.worker'
        w.openupdate()
    ]=]):format(
          root..path
        , root..cpath
        , root..'/error.log'
        , addr
        , client == true and "true" or "false"
    )
    rdebug.start(bootstrap)
end

function m:wait(name, ...)
    rdebug.probe 'wait_client'
end

function m:event(name, ...)
    return rdebug.event('event_'..name, ...)
end

return m
