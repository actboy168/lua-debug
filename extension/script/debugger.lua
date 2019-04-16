local rdebug, debugpath, runtime = ...

local function start_worker(addr)
    local selfSource = debug.getinfo(1, 'S').source:sub(2)
    local bootstrap = ([=[
        local debug = %q
        local runtime = %q
        local addr = %q
        package.path = debug..'/script/?.lua'
        package.cpath = debug..runtime..'/?.dll'
        local m = require 'start_master'
        m(debug, runtime, addr)
        local w = require 'backend.worker'
        w.skipfiles{%q}
        w.openupdate()
    ]=]):format(debugpath, runtime, addr, selfSource)
    rdebug.start(bootstrap)
end

local m = {}

function m:io(addr)
    self._addr = addr
    return self
end

function m:wait()
    self._wait = true
    return self
end

function m:start()
    start_worker(self._addr)
    if self._wait then
        rdebug.probe 'wait_client'
    end
    return self
end

function m:guard()
    return self
end

function m:redirect()
    return self
end

function m:config()
    return self
end

function m:event()
    return self
end

function m:exception()
    return self
end

return m
