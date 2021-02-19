local rdebug = require 'remotedebug.visitor'
local json = require 'common.json'
local variables = require 'backend.worker.variables'
local source = require 'backend.worker.source'
local breakpoint = require 'backend.worker.breakpoint'
local evaluate = require 'backend.worker.evaluate'
local traceback = require 'backend.worker.traceback'
local stdout = require 'backend.worker.stdout'
local luaver = require 'backend.worker.luaver'
local ev = require 'backend.event'
local hookmgr = require 'remotedebug.hookmgr'
local stdio = require 'remotedebug.stdio'
local thread = require 'remotedebug.thread'
local fs = require 'backend.worker.filesystem'
local err = thread.channel 'errlog'

local initialized = false
local info = {}
local state = 'running'
local stopReason = 'step'
local exceptionFilters = {}
local currentException = {
    message = '',
    Trace = '',
}
local outputCapture = {}
local noDebug = false
local openUpdate = true
local coroutineTree = {}
local stackFrame = {}
local skipFrame = 0
local baseL

local CMD = {}

local WorkerId = tostring(hookmgr.gethost())
local WorkerChannel = ('DbgWorker(%s)'):format(WorkerId)

thread.newchannel (WorkerChannel)
local masterThread = thread.channel 'DbgMaster'
local workerThread = thread.channel (WorkerChannel)

local function workerThreadUpdate(timeout)
    while true do
        local ok, msg = workerThread:pop(timeout)
        if not ok then
            break
        end
        local ok, e = xpcall(function()
            local pkg = json.decode(msg)
            local f = CMD[pkg.cmd]
            if f then
                f(pkg)
            end
        end, debug.traceback)
        if not ok then
            err:push(e)
        end
    end
end

local function sendToMaster(msg)
	masterThread:push(WorkerId, assert(json.encode(msg)))
end

ev.on('breakpoint', function(reason, bp)
    sendToMaster {
        cmd = 'eventBreakpoint',
        reason = reason,
        breakpoint = bp,
    }
end)

ev.on('output', function(category, output, source, line)
    sendToMaster {
        cmd = 'eventOutput',
        category = category,
        output = output,
        source = source and {
            name = source.name,
            path = source.path,
            sourceReference = source.sourceReference,
        } or nil,
        line = line,
    }
end)

ev.on('loadedSource', function(reason, s)
    sendToMaster {
        cmd = 'loadedSource',
        reason = reason,
        source = source.output(s)
    }
end)

--function print(...)
--    local n = select('#', ...)
--    local t = {}
--    for i = 1, n do
--        t[i] = tostring(select(i, ...))
--    end
--    ev.emit('output', 'stderr', table.concat(t, '\t')..'\n')
--end

--local log = require 'common.log'
--print = log.info

local function cleanFrame()
    variables.clean()
    stackFrame = {}
end

function CMD.initializing(pkg)
    luaver.init()
    ev.emit('initializing', pkg.config)
end

function CMD.initialized()
    initialized = true
end

function CMD.terminated()
    if initialized then
        initialized = false
        state = 'running'
        ev.emit('terminated')
    end
end

function CMD.exit()
    if initialized then
        CMD.terminated()
        sendToMaster {
            cmd = 'exitThread',
        }
    end
end

local function getFuncName(depth)
    if info.what == 'main' then
        return '(main)'
    end
    if info.namewhat == '' then
        if luaver.LUAVERSION >= 52
            and info.what == "Lua"
            and rdebug.getinfo(depth, "t", info)
            and info.istailcall
        then
            return '(...tail calls...)'
        end
        local previous = {}
        if rdebug.getinfo(depth+1, "S", previous) then
            if previous.what == "Lua" or previous.what == "main" then
                return '(anonymous function)'
            end
            if previous.what == "C" then
                return '(called from C)'
            end
        end
        return ('(%s ?)'):format(info.what)
    end
    if info.namewhat == 'for iterator' then
        return '(for iterator)'
    end
    if info.namewhat == 'hook' then
        return '(hook)'
    end
    if info.namewhat == 'metamethod' then
        return ('(metamethod %s)'):format(info.name)
    end
    if info.namewhat == 'field' and info.name == 'integer index' then
        return '(field ?)'
    end
    if info.name == '?' then
        return ('(%s ?)'):format(info.namewhat)
    end
    return info.name
end

local function stackTrace(res, coid, start, levels)
    for depth = start, start + levels - 1 do
        if not rdebug.getinfo(depth, "Sln", info) then
            return true, depth - start
        end
        local r = {
            id = (coid << 16) | depth,
            name = getFuncName(depth),
            line = 0,
            column = 0,
        }
        if info.what ~= 'C' then
            r.column = 1
            local src = source.create(info.source)
            if source.valid(src) then
                r.line = source.line(src, info.currentline)
                r.source = source.output(src)
                r.presentationHint = 'normal'
            else
                r.line = info.currentline
                r.presentationHint = 'label'
            end
        else
            r.presentationHint = 'label'
        end
        res[#res + 1] = r
    end
    return false, levels
end

local function skipCFunction(res)
    for i = #res, 1, -1 do
        if res[i].presentationHint ~= "label" or res[i].name ~= "(C ?)" then
            break
        end
        res[i] = nil
    end
end

function CMD.stackTrace(pkg)
    local start = pkg.startFrame and pkg.startFrame or 0
    local levels = (pkg.levels and pkg.levels ~= 0) and pkg.levels or 200
    local res = {}

    --
    -- 在VSCode的实现中这是一帧的第一个请求，所以在这里清理上一帧的数据。
    -- 很特殊，但目前也只能这样。
    --
    if start == 0 and levels == 1 then
        cleanFrame()
    end

    start = start + skipFrame
    local L = baseL
    local coroutineId = 0
    repeat
        hookmgr.sethost(L)
        local curL = L
        L = coroutineTree[curL]
        if stackFrame[curL] == nil then
            local finsh, n = stackTrace(res, coroutineId, start, levels)
            if not finsh then
                break
            end
            if not L then
                skipCFunction(res)
                break
            end
            stackFrame[curL] = start + n
            start = 0
            levels = levels - n
        elseif start > stackFrame[curL] then
            start = start - stackFrame[curL]
        end
        coroutineId = coroutineId + 1
    until (not L or levels <= 0)
    hookmgr.sethost(baseL)

    sendToMaster {
        cmd = 'stackTrace',
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        stackFrames = res,
        totalFrames = 0x10000,
    }
end

function CMD.source(pkg)
    sendToMaster {
        cmd = 'source',
        command = pkg.command,
        seq = pkg.seq,
        content = source.getCode(pkg.sourceReference),
    }
end

local function findFrame(id)
    local L = baseL
    for _ = 1, id - 1 do
        if not L then
            return
        end
        L = coroutineTree[L]
    end
    return L
end

function CMD.scopes(pkg)
    local coid = (pkg.frameId >> 16) + 1
    local depth = pkg.frameId & 0xFFFF
    hookmgr.sethost(assert(findFrame(coid)))
    sendToMaster {
        cmd = 'scopes',
        command = pkg.command,
        seq = pkg.seq,
        scopes = variables.scopes(depth),
    }
end

function CMD.variables(pkg)
    local vars, err = variables.extand(pkg.valueId, pkg.filter, pkg.start, pkg.count)
    if not vars then
        sendToMaster {
            cmd = 'variables',
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster {
        cmd = 'variables',
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        variables = vars,
    }
end

function CMD.setVariable(pkg)
    local var, err = variables.set(pkg.valueId, pkg.name, pkg.value)
    if not var then
        sendToMaster {
            cmd = 'setVariable',
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster {
        cmd = 'setVariable',
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        value = var.value,
        type = var.type,
    }
end

function CMD.evaluate(pkg)
    local depth = pkg.frameId & 0xFFFF
    local ok, result = evaluate.run(depth, pkg.expression, pkg.context)
    if not ok then
        sendToMaster {
            cmd = 'evaluate',
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = result,
        }
        return
    end
    sendToMaster {
        cmd = 'evaluate',
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = result
    }
end

function CMD.setBreakpoints(pkg)
    if noDebug or not source.valid(pkg.source) then
        return
    end
    breakpoint.set_bp(pkg.source, pkg.breakpoints, pkg.content)
end

function CMD.setFunctionBreakpoints(pkg)
    breakpoint.set_funcbp(pkg.breakpoints)
end

function CMD.setExceptionBreakpoints(pkg)
    exceptionFilters = {}
    for _, filter in ipairs(pkg.arguments.filters) do
        exceptionFilters[filter] = true
    end
    for _, filter in ipairs(pkg.arguments.filterOptions) do
        if type(filter.condition) == "string" and evaluate.verify(filter.condition) then
            exceptionFilters[filter.filterId] = filter.condition
        else
            exceptionFilters[filter.filterId] = true
        end
    end
    if hookmgr.exception_open then
        hookmgr.exception_open(next(exceptionFilters) ~= nil)
    end
end

function CMD.exceptionInfo(pkg)
    sendToMaster {
        cmd = 'exceptionInfo',
        command = pkg.command,
        seq = pkg.seq,
        breakMode = 'always',
        exceptionId = currentException.message,
        details = {
            stackTrace = currentException.trace,
        }
    }
end

function CMD.loadedSources()
    source.all_loaded()
end

function CMD.stop(pkg)
    if noDebug then
        return
    end
    state = 'stopped'
    stopReason = pkg.reason -- entry or pause
    hookmgr.step_in()
end

function CMD.run()
    state = 'running'
    hookmgr.step_cancel()
end

function CMD.stepOver()
    if noDebug then
        return
    end
    state = 'stepOver'
    hookmgr.step_over()
end

function CMD.stepIn()
    if noDebug then
        return
    end
    state = 'stepIn'
    hookmgr.step_in()
end

function CMD.stepOut()
    if noDebug then
        return
    end
    state = 'stepOut'
    hookmgr.step_out()
end

function CMD.restartFrame()
    cleanFrame()
    sendToMaster {
        cmd = 'eventStop',
        reason = 'restart',
    }
end

function CMD.setSearchPath(pkg)
    local function set_search_path(name)
        if not pkg[name] or pkg[name] == json.null then
            return
        end
        local value = pkg[name]
        if type(value) == 'table' then
            local path = {}
            for _, v in ipairs(value) do
                if type(v) == "string" then
                    path[#path+1] = fs.nativepath(v)
                end
            end
            value = table.concat(path, ";")
        else
            value = fs.nativepath(value)
        end
        local visitor = rdebug.field(rdebug.field(rdebug._G, "package"), name)
        if not rdebug.assign(visitor, value) then
            return
        end
    end
    set_search_path "path"
    set_search_path "cpath"
end

function CMD.customRequestShowIntegerAsDec()
    variables.showIntegerAsDec()
    sendToMaster {
        cmd = "eventInvalidated",
        areas = "variables",
    }
end

function CMD.customRequestShowIntegerAsHex()
    variables.showIntegerAsHex()
    sendToMaster {
        cmd = "eventInvalidated",
        areas = "variables",
    }
end

local function runLoop(reason, text, level)
    baseL = hookmgr.gethost()
    --TODO: 只在lua栈帧时需要text？
    sendToMaster {
        cmd = 'eventStop',
        reason = reason,
        text = text,
    }
    skipFrame = level or 0

    while true do
        workerThreadUpdate(0.01)
        if state ~= 'stopped' then
            break
        end
    end
end

local event = {}

local function event_breakpoint(src, line)
    if not source.valid(src) then
        hookmgr.break_closeline()
        return
    end
    local bp = breakpoint.find(src, source.line(src, line))
    if bp then
        if breakpoint.exec(bp) then
            state = 'stopped'
            runLoop 'breakpoint'
            return true
        end
    end
end

function event.bp(line)
    if not initialized then return end
    rdebug.getinfo(0, "S", info)
    local src = source.create(info.source)
    event_breakpoint(src, line)
end

function event.funcbp(func)
    if not initialized then return end
    if breakpoint.hit_funcbp(func) then
        state = 'stopped'
        runLoop 'function breakpoint'
    end
end

function event.step(line)
    if not initialized then return end
    rdebug.getinfo(0, "S", info)
    local src = source.create(info.source)
    if event_breakpoint(src, line) then
        return
    end
    if not source.valid(src) then
        return
    end
    workerThreadUpdate()
    if state == 'running' then
        return
    elseif state == 'stepOver' or state == 'stepOut' or state == 'stepIn' then
        state = 'stopped'
        stopReason = 'step'
        hookmgr.step_cancel()
    end
    if state == 'stopped' then
        runLoop(stopReason)
    end
end

function event.newproto(proto, level)
    if not initialized then return end
    rdebug.getinfo(level, "S", info)
    local src = source.create(info.source)
    if not source.valid(src) then
        return false
    end
    return breakpoint.newproto(proto, src, info.linedefined.."-"..info.lastlinedefined)
end

local function pairsEventArgs()
    local max = rdebug.getstack()
    local n = 1
    return function()
        n = n + 1
        if n > max then
            return
        end
        return n, rdebug.getstack(n)
    end
end

function event.update()
    workerThreadUpdate()
end

function event.enable_update(flag)
    openUpdate = flag
    hookmgr.update_open(not noDebug and openUpdate)
end

function event.print()
    if not initialized then return end
    local res = {}
    for _, arg in pairsEventArgs() do
        res[#res + 1] = variables.tostring(arg)
    end
    res = table.concat(res, '\t') .. '\n'
    rdebug.getinfo(1, "Sl", info)
    local src = source.create(info.source)
    if source.valid(src) then
        stdout(res, src, source.line(src, info.currentline))
    else
        stdout(res)
    end
    return true
end

function event.iowrite()
    if not initialized then return end
    local res = {}
    for _, arg in pairsEventArgs() do
        res[#res + 1] = variables.tostring(arg)
    end
    res = table.concat(res, '\t')
    rdebug.getinfo(1, "Sl", info)
    local src = source.create(info.source)
    if source.valid(src) then
        stdout(res, src, source.line(src, info.currentline))
    else
        stdout(res)
    end
    return true
end

local function execExceptionBreakpoint(type, level, error)
    local filter = exceptionFilters[type]
    if filter == true or filter == nil then
        return filter
    end
    local ok, res = evaluate.eval(filter, level, { error = error })
    return (not ok) or res
end

local function getExceptionType()
    local pcall = rdebug.value(rdebug.fieldv(rdebug._G, 'pcall'))
    local xpcall = rdebug.value(rdebug.fieldv(rdebug._G, 'xpcall'))
    local level = 1
    while true do
        local f = rdebug.getfunc(level)
        if f == nil then
            break
        end
        f = rdebug.value(f)
        if f == pcall then
            return 'pcall'
        end
        if f == xpcall then
            return 'xpcall'
        end
        level = level + 1
    end
    return 'lua_pcall'
end

local function runException(type, error)
    local level, message, trace = traceback(error)
    if level < 0 or not execExceptionBreakpoint(type, level, error) then
        return
    end
    currentException = {
        message = message,
        trace = trace,
    }
    state = 'stopped'
    runLoop('exception', message, level)
end

function event.panic(error)
    if not initialized then return end
    runException('lua_panic', error)
end

function event.exception(error)
    if not initialized then return end
    runException(getExceptionType(), error)
end

function event.thread(co, type)
    local L = hookmgr.gethost()
    if co then
        if type == 0 then
            coroutineTree[L] = co
        elseif type == 1 then
            coroutineTree[co] = nil
        end
    end
    hookmgr.updatehookmask(L)
end

function event.wait()
    while not initialized do
        workerThreadUpdate(0.01)
    end
end

function event.exit()
    CMD.exit()
end

hookmgr.init(function(name, ...)
    local ok, e = xpcall(function(...)
        if event[name] then
            return event[name](...)
        end
    end, debug.traceback, ...)
    if not ok then
        err:push(e)
        return
    end
    return e
end)

local function lst2map(t)
    local r = {}
    for _, v in ipairs(t) do
        r[v] = true
    end
    return r
end

ev.on('initializing', function(config)
    noDebug = config.noDebug
    hookmgr.update_open(not noDebug and openUpdate)
    if hookmgr.thread_open then
        hookmgr.thread_open(true)
    end
    outputCapture = lst2map(config.outputCapture)
    if outputCapture["print"] then
        stdio.open_print(true)
    end
    if outputCapture["io.write"] then
        stdio.open_iowrite(true)
    end
end)

ev.on('terminated', function()
    hookmgr.step_cancel()
    if outputCapture["print"] then
        stdio.open_print(false)
    end
    if outputCapture["io.write"] then
        stdio.open_iowrite(false)
    end
end)

sendToMaster {
    cmd = 'startThread',
}

hookmgr.update_open(true)
