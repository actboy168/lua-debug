local ev = require 'backend.event'
local rdebug = require 'luadebug.visitor'
local luaver = require 'backend.worker.luaver'
local hookmgr = require 'luadebug.hookmgr'

local enable_jit = false
local no_vmevent = false

local function set_jitmode(on)
    on = on and "on" or "off"
    local code = ([[
            if jit and jit.%s then
                jit.%s()
            end
        ]]):format(on, on)
    local thunk = rdebug.load(code)
    if thunk then
        rdebug.eval(thunk)
    end
end

local function attach_vmevent()
    local eval = require 'backend.worker.eval'
    return eval.vmevent(true)
end

local function detach_vmevent()
    local eval = require 'backend.worker.eval'
    eval.vmevent(false)
end

ev.on('initializing', function (config)
    if not luaver.isjit then
        return
    end
    enable_jit = no_vmevent or not config.disable_jit
    if not enable_jit then
        set_jitmode(false)
        return
    end
    if not attach_vmevent() then
        no_vmevent = true
        return
    end
    local worker = require 'backend.worker'
    worker.autoUpdate(false)
    hookmgr.enable_jit(true)
end)

local function terminated()
    if not luaver.isjit then
        return
    end
    if not enable_jit then
        set_jitmode(true)
        return
    end
    detach_vmevent()
    hookmgr.enable_jit(false)
end

ev.on('terminated', terminated)
