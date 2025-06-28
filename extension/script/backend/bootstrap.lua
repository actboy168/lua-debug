local thread = require "bee.thread"
local channel = require "bee.channel"
local tag = require 'backend.tag'
local m = {}

local function hasMaster()
    local ok, masterChannel = pcall(channel.query, tag.getChannelKeyMaster())
    return ok and masterChannel
end

local function initMaster(rootpath, address)
    if hasMaster() then
        return
    end
    local chan = channel.create (tag.getChannelKeyMaster())
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
        channel.destroy("DbgMaster")
    end})
end

local function startWorker(rootpath)
    local log = require 'common.log'
    log.file = rootpath..'/worker.log'
    require 'backend.worker'
end

local function startDebugger(rootpath)
    local function mydofile(filename)
        local load = _VERSION == "Lua 5.1" and loadstring or load
        local f = assert(io.open(filename))
        local str = f:read "*a"
        f:close()
        return assert(load(str, "=(debugger.lua)"))(filename)
    end
    ---@module 'script.debugger'
    local debugger = mydofile(rootpath.."/script/debugger.lua")

    debugger:start({ address = ('127.0.0.1:44711'):format(rootpath), debug_debugger = true })

end

function m.start(rootpath, address, worker_tag, debug_debugger)
    require 'backend.tag'.setTag(worker_tag)
    initMaster(rootpath, address)
    startWorker(rootpath)
    if debug_debugger == 'worker' then
        startDebugger(rootpath)
    end
end

function m.attach(rootpath, worker_tag)
    require 'backend.tag'.setTag(worker_tag)
    if hasMaster() then
        startWorker(rootpath)
    end
end

return m
