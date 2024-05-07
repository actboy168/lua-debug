local thread = require "bee.thread"
local channel = require "bee.channel"

local m = {}

local function hasMaster()
    local ok = pcall(channel.query, "DbgMaster")
    return ok
end

local function initMaster(rootpath, address)
    if hasMaster() then
        return
    end
    local chan = channel.create "DbgMaster"
    local mt = thread.create(([[
        local rootpath = %q
        package.path = rootpath.."/script/?.lua"
        local log = require "common.log"
        log.file = rootpath.."/master.log"
        local ok, err = xpcall(function()
            local socket = require "common.socket"(%q)
            local master = require "backend.master.mgr"
            master.init(socket)
            master.update()
        end, debug.traceback)
        if not ok then
            log.error("ERROR:" .. err)
        end
    ]]):format(
        rootpath,
        address
    ))
    ExitGuard = setmetatable({}, {__gc=function()
        chan:push(nil, "EXIT")
        thread.wait(mt)
    end})
end

local function startWorker(rootpath)
    local log = require 'common.log'
    log.file = rootpath..'/worker.log'
    require 'backend.worker'
end

function m.start(rootpath, address)
    initMaster(rootpath, address)
    startWorker(rootpath)
end

function m.attach(rootpath)
    if hasMaster() then
        startWorker(rootpath)
    end
end

return m
