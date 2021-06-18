local mgr = require 'backend.master.mgr'
local response = require 'backend.master.response'
local event = require 'backend.master.event'
local ev = require 'backend.event'
local utility = require 'remotedebug.utility'

local request = {}

local firstWorker = true
local closeProcess = false
local state = 'none'
local config = {
    initialize = {},
    breakpoints = {},
    function_breakpoints = {},
    exception_breakpoints = {},
}

ev.on('close', function()
    state = "none"
    event.terminated()
end)

local function checkThreadId(req, threadId)
    if type(threadId) ~= 'number' then
        response.error(req, "No threadId")
        return
    end
    if not mgr.hasThread(threadId) then
        response.error(req, "Not found thread [" .. threadId .. "]")
        return
    end
    return true
end

function request.initialize(req)
    firstWorker = true
    mgr.setClient(req.arguments)
    response.initialize(req)
    event.initialized()
    event.capabilities()
end

function request.attach(req)
    response.success(req)
    state = "initializing"
    config = {
        initialize = req.arguments,
        breakpoints = {},
        function_breakpoints = {},
        exception_breakpoints = {},
    }
end

function request.launch(req)
    request.attach(req)
    config.launch = true
end

local function tryStop(w)
    if firstWorker then
        if not not config.initialize.stopOnEntry then
            mgr.sendToWorker(w, {
                cmd = 'stop',
                reason = 'entry',
            })
            return
        end
    end
    if not not config.initialize.stopOnThreadEntry then
        mgr.sendToWorker(w, {
            cmd = 'stop',
            reason = 'entry',
        })
    end
end

local function initializeWorkerBreakpoints(w, source, breakpoints, content)
    mgr.sendToWorker(w, {
        cmd = 'setBreakpoints',
        source = source,
        breakpoints = breakpoints,
        content = content,
    })
end

local function initializeWorker(w)
    mgr.sendToWorker(w, {
        cmd = 'initializing',
        config = config.initialize,
    })
    for key, bp in pairs(config.breakpoints) do
        if type(key) == "string" or (key >> 32) == w then
            initializeWorkerBreakpoints(w, bp[1], bp[2], bp[3])
        end
    end
    mgr.sendToWorker(w, {
        cmd = 'setFunctionBreakpoints',
        breakpoints = config.function_breakpoints,
    })
    mgr.sendToWorker(w, {
        cmd = 'setExceptionBreakpoints',
        arguments = config.exception_breakpoints,
    })
    if firstWorker and config.launch then
        mgr.sendToWorker(w, {
            cmd = 'setSearchPath',
            path = config.initialize.path,
            cpath = config.initialize.cpath,
        })
    end
    tryStop(w)
    mgr.sendToWorker(w, {
        cmd = 'initialized',
    })
    firstWorker = false
end

ev.on('worker-ready', function(w)
    if state == "initialized" then
        initializeWorker(w)
    end
end)

function request.configurationDone(req)
    response.success(req)
    state = "initialized"
    for w in pairs(mgr.workers()) do
        initializeWorker(w)
    end
    mgr.initConfig(config)
end

local breakpointID = 0
local function genBreakpointID()
    breakpointID = breakpointID + 1
    return breakpointID
end

local function skipBOM(s)
    if not s then
        return
    end
    if s:sub(1,3) == "\xEF\xBB\xBF" then
        s = s:sub(4)
    end
    if s:sub(1,1) == "#" then
        local pos = s:find('\r\n', 2, true)
        s = pos and s:sub(pos+1) or s
    end
    return s
end

local function isValidPath(path)
    local prefix = path:match "^(%a+):"
    return not (prefix and #prefix > 1)
end

function request.setBreakpoints(req)
    local args = req.arguments
    local invalidPath = args.source.path and not isValidPath(args.source.path)
    for _, bp in ipairs(args.breakpoints) do
        bp.id = genBreakpointID()
        bp.verified = false
        bp.message = invalidPath
                    and ("Does not support path: `%s`"):format(args.source.path)
                    or "Wait verify. (The source file is not loaded.)"
    end
    response.success(req, {
        breakpoints = args.breakpoints
    })
    if invalidPath then
        return
    end
    local content = skipBOM(args.sourceContent)
    if args.source.sourceReference then
        local sourceReference = args.source.sourceReference
        local w = sourceReference >> 32
        args.source.sourceReference = args.source.sourceReference & 0xFFFFFFFF
        config.breakpoints[sourceReference] = {
            args.source,
            args.breakpoints,
            content,
        }
        if state == "initialized" then
            initializeWorkerBreakpoints(w, args.source, args.breakpoints, content)
        end
    else
        --TODO path 无视大小写？
        config.breakpoints[args.source.path] = {
            args.source,
            args.breakpoints,
            content,
        }
        if state == "initialized" then
            for w in pairs(mgr.workers()) do
                initializeWorkerBreakpoints(w, args.source, args.breakpoints, content)
            end
        end
    end
end

function request.setFunctionBreakpoints(req)
    local args = req.arguments
    for _, bp in ipairs(args.breakpoints) do
        bp.id = genBreakpointID()
        bp.verified = false
        bp.message = "Wait verify."
    end
    response.success(req, {
        breakpoints = args.breakpoints
    })
    config.function_breakpoints = args.breakpoints
    if state == "initialized" then
        mgr.broadcastToWorker {
            cmd = 'setFunctionBreakpoints',
            breakpoints = args.breakpoints,
        }
    end
end

function request.setExceptionBreakpoints(req)
    local args = req.arguments
    local breakpoints = {}
    local filter = {}
    local function addExceptionBreakpoint(opt)
        local id = genBreakpointID()
        breakpoints[#breakpoints+1] = {
            id = id,
            verified = false,
            message = "Wait verify."
        }
        filter[#filter+1] = {
            id = id,
            filterId = opt.filterId,
            condition = opt.condition,
        }
    end
    for _, filterId in ipairs(args.filters) do
        addExceptionBreakpoint {
            filterId = filterId
        }
    end
    if args.filterOptions then
        for _, opt in ipairs(args.filterOptions) do
            addExceptionBreakpoint(opt)
        end
    end
    response.success(req, {
        breakpoints = breakpoints,
    })
    config.exception_breakpoints = filter
    if state == "initialized" then
        mgr.broadcastToWorker {
            cmd = 'setExceptionBreakpoints',
            arguments = filter,
        }
    end
end

function request.stackTrace(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'stackTrace',
        command = req.command,
        seq = req.seq,
        startFrame = args.startFrame,
        levels = args.levels,
    })
end

function request.scopes(req)
    local args = req.arguments
    if type(args.frameId) ~= 'number' then
        response.error(req, "No frameId")
        return
    end

    local threadId = args.frameId >> 24
    local frameId = args.frameId & 0x00FFFFFF
    if not checkThreadId(req, threadId) then
        return
    end

    mgr.sendToWorker(threadId, {
        cmd = 'scopes',
        command = req.command,
        seq = req.seq,
        frameId = frameId,
    })
end

function request.variables(req)
    local args = req.arguments
    local threadId = args.variablesReference >> 24
    local valueId = args.variablesReference & 0x00FFFFFF
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'variables',
        command = req.command,
        seq = req.seq,
        valueId = valueId,
        filter = args.filter,
        start = args.start,
        count = args.count,
    })
end

function request.evaluate(req)
    local args = req.arguments
    if type(args.frameId) ~= 'number' then
        response.error(req, "Please pause to evaluate expressions")
        return
    end
    if type(args.expression) ~= 'string' then
        response.error(req, "Error expression")
        return
    end
    local threadId = args.frameId >> 24
    local frameId = args.frameId & 0x00FFFFFF
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'evaluate',
        command = req.command,
        seq = req.seq,
        frameId = frameId,
        context = args.context,
        expression = args.expression,
    })
end

function request.threads(req)
    response.threads(req, mgr.threads())
end

function request.disconnect(req)
    response.success(req)
    local args = req.arguments
    if args.terminateDebuggee == nil then
        args.terminateDebuggee = not not config.launch
    end
    mgr.broadcastToWorker {
        cmd = 'disconnect',
    }
    if args.terminateDebuggee then
        if closeProcess then
            mgr.setTerminateDebuggeeCallback(function()
                os.exit(true, true)
            end)
        else
            os.exit(true, true)
        end
    end
    return true
end

function request.terminate(req)
    response.success(req)
    if utility.closewindow() then
        return
    end
    --TODO:
    --  现在调试器激活是会屏蔽SIGINT，导致closeprocess无法生效，所以需要先将调试器关闭，再调用closeprocess。
    --  或许需要让调试器和SIGINT不再冲突。
    --
    mgr.broadcastToWorker {
        cmd = 'disconnect',
    }
    mgr.setTerminateDebuggeeCallback(function()
        closeProcess = true
        utility.closeprocess()
    end)
    return true
end

function request.restart(req)
    local args = req.arguments.arguments
    response.success(req)
    mgr.broadcastToWorker {
        cmd = 'disconnect',
    }
    mgr.setTerminateDebuggeeCallback(function()
        if args then
            config.initialize = args
        end
        for w in pairs(mgr.workers()) do
            initializeWorker(w)
        end
        mgr.initConfig(config)
    end)
end

function request.terminateThreads(req)
    local args = req.arguments
    response.success(req)
    for _, w in ipairs(args.threadIds) do
        mgr.sendToWorker(w, {
            cmd = 'disconnect',
        })
    end
end

function request.pause(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'stop',
        reason = 'pause',
    })
    response.success(req)
end

function request.continue(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'run',
    })
    response.success(req, {
        allThreadsContinued = false,
    })
end

function request.next(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'stepOver',
    })
    response.success(req)
end

function request.stepOut(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'stepOut',
    })
    response.success(req)
end

function request.stepIn(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'stepIn',
    })
    response.success(req)
end

function request.source(req)
    local args = req.arguments
    local threadId = args.sourceReference >> 32
    local sourceReference = args.sourceReference & 0xFFFFFFFF
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'source',
        command = req.command,
        seq = req.seq,
        sourceReference = sourceReference,
    })
end

function request.exceptionInfo(req)
    local args = req.arguments
    local threadId = args.threadId
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'exceptionInfo',
        command = req.command,
        seq = req.seq,
    })
end

function request.setVariable(req)
    local args = req.arguments
    local threadId = args.variablesReference >> 24
    local valueId = args.variablesReference & 0x00FFFFFF
    if not checkThreadId(req, threadId) then
        return
    end
    mgr.sendToWorker(threadId, {
        cmd = 'setVariable',
        command = req.command,
        seq = req.seq,
        valueId = valueId,
        name = args.name,
        value = args.value,
    })
end

function request.loadedSources(req)
    response.success(req, {
        sources = {}
    })
    mgr.broadcastToWorker {
        cmd = 'loadedSources'
    }
end

function request.restartFrame(req)
    local args = req.arguments
    local threadId = args.frameId >> 24
    local frameId = args.frameId & 0x00FFFFFF
    if not checkThreadId(req, threadId) then
        return
    end
    response.success(req)
    mgr.sendToWorker(threadId, {
        cmd = 'restartFrame',
        frameId = frameId,
    })
end

function request.customRequestShowIntegerAsDec(req)
    response.success(req)
    mgr.broadcastToWorker {
        cmd = 'customRequestShowIntegerAsDec'
    }
end

function request.customRequestShowIntegerAsHex(req)
    response.success(req)
    mgr.broadcastToWorker {
        cmd = 'customRequestShowIntegerAsHex'
    }
end

--function print(...)
--    local n = select('#', ...)
--    local t = {}
--    for i = 1, n do
--        t[i] = tostring(select(i, ...))
--    end
--    event.output {
--        category = 'stdout',
--        output = table.concat(t, '\t')..'\n',
--    }
--end

--local log = require 'common.log'
--print = log.info

return request
