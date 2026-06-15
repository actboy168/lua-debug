local rdebug = require 'luadebug.visitor'
local variables = require 'backend.worker.variables'
local source = require 'backend.worker.source'
local breakpoint = require 'backend.worker.breakpoint'
local evaluate = require 'backend.worker.evaluate'
local traceback = require 'backend.worker.traceback'
local stdout = require 'backend.worker.stdout'
local luaver = require 'backend.worker.luaver'
local ev = require 'backend.event'
local hookmgr = require 'luadebug.hookmgr'
local stdio = require 'luadebug.stdio'
local thread = require 'bee.thread'
local fs = require 'backend.worker.filesystem'
local log = require 'common.log'
local channel = require "bee.channel"
local disassemble = require 'backend.worker.disassemble'
local initialized = false
local suspend = false
local info = {}
local state = 'running'
local stopReason = 'step'
local currentException = {
    message = '',
    trace = '',
}
local outputCapture = {}
local noDebug = false
local autoUpdate = true
local coroutineTree = {}
local stackFrame = {}
local skipFrame = 0
local baseL
local CMD = {}

local WorkerIdent = tostring(thread.id)
local WorkerChannel = ('DbgWorker(%s)'):format(WorkerIdent)

local masterThread = assert(channel.query 'DbgMaster')
local workerThread = channel.create(WorkerChannel)

local function workerThreadUpdate(timeout)
    while true do
        local ok, msg = workerThread:pop()
        if not ok then
            break
        end
        local ok, err = xpcall(function()
            local f = CMD[msg.cmd]
            if f then
                f(msg)
            end
        end, debug.traceback)
        if not ok then
            log.error("ERROR:"..err)
        end
    end
    if timeout then
        --TODO
        thread.sleep(timeout)
    end
end

local function sendToMaster(cmd)
    return function(msg)
        masterThread:push(WorkerIdent, cmd, msg)
    end
end

ev.on('breakpoint', function(reason, bp)
    sendToMaster 'eventBreakpoint' {
        reason = reason,
        breakpoint = bp,
    }
end)

ev.on('output', function(body)
    sendToMaster 'eventOutput' (body)
end)

ev.on('loadedSource', function(reason, s)
    local src = source.output(s)
    if src then
        sendToMaster 'eventLoadedSource' {
            reason = reason,
            source = src
        }
    end
end)

ev.on('memory', function(memoryReference, offset, count)
    sendToMaster 'eventMemory' {
        memoryReference = memoryReference,
        offset = offset,
        count = count,
    }
end)

--function print(...v)
--    local t = {}
--    for i = 1, #v do
--        t[i] = tostring(v[i])
--    end
--    ev.emit('output', {
--        category = 'stderr',
--        output = table.concat(t, '\t')..'\n',
--    })
--end

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
    suspend = false
    sendToMaster 'eventThread' {
        reason = 'started',
    }
end

function CMD.disconnect()
    if initialized then
        initialized = false
        state = 'running'
        ev.emit('terminated')
        sendToMaster 'eventThread' {
            reason = 'exited'
        }
    end
end

function CMD.suspend()
    suspend = true
end

local function getFuncName(depth)
    local funcname = traceback.pushglobalfuncname(info.func)
    if funcname then
        return funcname
    end
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
        if rdebug.getinfo(depth + 1, "S", previous) then
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
        if not rdebug.getinfo(depth, "Slnf", info) then
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
                if rdebug.currentpc then
                    local pc = rdebug.currentpc(depth)
                    if pc >= 0 then
                        local srcId = src.sourceReference and tostring(src.sourceReference) or fs.path_native(fs.path_normalize(src.path))
                        r.instructionPointerReference = ("bp_%d_%d_%d_%s"):format(
                            info.linedefined, info.lastlinedefined, pc, srcId
                        )
                    end
                end
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

local function coroutineFrom(L)
    if hookmgr.coroutine_from then
        return coroutineTree[L] or hookmgr.coroutine_from(L)
    end
    return coroutineTree[L]
end

local function nextTotalFrames(finish, n)
    if finish then
        return
    end
    n = n + 0x10
    if n >= 0x10000 then
        return
    end
    local p = 64
    while p < n do
        p = p * 2
    end
    return p
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
    local finish
    repeat
        hookmgr.sethost(L)
        local curL = L
        L = coroutineFrom(curL)
        if stackFrame[curL] == nil then
            local n;
            finish, n = stackTrace(res, coroutineId, start, levels)
            if not finish then
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

    -- TODO 当frames很多时，跳过中间的部分
    sendToMaster 'stackTrace' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = {
            stackFrames = res,
            totalFrames = nextTotalFrames(finish, start + levels),
        }
    }
end

function CMD.source(pkg)
    sendToMaster 'source' {
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
        L = coroutineFrom(L)
    end
    return L
end

function CMD.scopes(pkg)
    local coid = (pkg.frameId >> 16) + 1
    local depth = pkg.frameId & 0xFFFF
    hookmgr.sethost(assert(findFrame(coid)))
    sendToMaster 'scopes' {
        command = pkg.command,
        seq = pkg.seq,
        body = {
            scopes = variables.scopes(depth)
        }
    }
end

function CMD.variables(pkg)
    local vars, err = variables.extand(pkg.valueId, pkg.filter, pkg.start, pkg.count)
    if not vars then
        sendToMaster 'variables' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster 'variables' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = {
            variables = vars
        }
    }
end

function CMD.setVariable(pkg)
    local var, err = variables.set(pkg.valueId, pkg.name, pkg.value)
    if not var then
        sendToMaster 'setVariable' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster 'setVariable' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = {
            value = var.value,
            type = var.type,
        }
    }
end

function CMD.readMemory(pkg)
    local res, err = variables.readMemory(pkg.memoryReference, pkg.offset, pkg.count)
    if not res then
        sendToMaster 'readMemory' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster 'readMemory' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = res
    }
end

function CMD.writeMemory(pkg)
    local res, err = variables.writeMemory(pkg.memoryReference, pkg.offset, pkg.data, pkg.allowPartial)
    if not res then
        sendToMaster 'writeMemory' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = err,
        }
        return
    end
    sendToMaster 'writeMemory' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = res
    }
end

function CMD.setExpression(pkg)
    local depth = pkg.frameId & 0xFFFF
    local ok, result = evaluate.set(depth, pkg.expression, pkg.value)
    if not ok then
        sendToMaster 'setExpression' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = result,
        }
        return
    end
    sendToMaster 'setExpression' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = result
    }
end

function CMD.evaluate(pkg)
    local depth = pkg.frameId & 0xFFFF
    local ok, result = evaluate.run(depth, pkg.expression, pkg.context)
    if not ok then
        sendToMaster 'evaluate' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = result,
        }
        return
    end
    sendToMaster 'evaluate' {
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
    breakpoint.set_bp(pkg.source, pkg.breakpoints, pkg.content or false)
end

function CMD.setFunctionBreakpoints(pkg)
    breakpoint.set_funcbp(pkg.breakpoints)
end

function CMD.setInstructionBreakpoints(pkg)
    if noDebug then
        return
    end
    breakpoint.set_instbp(pkg.breakpoints)
end

function CMD.setExceptionBreakpoints(pkg)
    breakpoint.setExceptionBreakpoints(pkg.arguments)
end

function CMD.exceptionInfo(pkg)
    sendToMaster 'exceptionInfo' {
        command = pkg.command,
        seq = pkg.seq,
        body = {
            breakMode = 'always',
            exceptionId = currentException.message,
            details = {
                stackTrace = currentException.trace,
            }
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
    sendToMaster 'eventStop' {
        reason = 'restart',
    }
end

function CMD.setSearchPath(pkg)
    local function set_search_path(name)
        if not pkg[name] then
            return
        end
        local value = pkg[name]
        if type(value) == 'table' then
            local path = {}
            for _, v in ipairs(value) do
                if type(v) == "string" then
                    path[#path + 1] = fs.nativepath(v)
                end
            end
            value = table.concat(path, ";")
        else
            value = fs.nativepath(value)
        end
        local visitor = rdebug.field(rdebug._G, "package")
        if visitor == nil then
            return
        end
        if not rdebug.assign_field(visitor, name, value) then
            return
        end
    end
    set_search_path "path"
    set_search_path "cpath"
end

function CMD.customRequestShowIntegerAsDec()
    variables.showIntegerAsDec()
    sendToMaster 'eventInvalidated' {
        areas = "variables",
    }
end

function CMD.customRequestShowIntegerAsHex()
    variables.showIntegerAsHex()
    sendToMaster 'eventInvalidated' {
        areas = "variables",
    }
end

function CMD.disassemble(pkg)
    if not rdebug.dumpproto then
        sendToMaster 'disassemble' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = "Disassemble not supported for this Lua version",
        }
        return
    end
    local proto
    local defaultOffset = 0
    local refId = pkg.refId
    local ld, lld, pc, srcId = refId:match("^bp_(%d+)_(%d+)_(%d+)_(.+)$")
    if ld then
        local funcId = ld .. "-" .. lld .. "_" .. srcId
        local proto_ptr = breakpoint.get_proto(funcId)
        if proto_ptr then
            proto = rdebug.dumpproto(proto_ptr)
        else
            proto, proto_ptr = rdebug.dumpproto(0)
            if not (proto and proto.linedefined == tonumber(ld) and proto.lastlinedefined == tonumber(lld)) then
                proto = nil
            end
        end
        defaultOffset = tonumber(pc)
    end
    if not proto then
        local rtype, ref = variables.resolveMemoryRef(tonumber(refId))
        if ref and rtype == "function" then
            proto = rdebug.dumpproto(ref)
        end
    end
    if refId and refId:match("^bp_") and pkg.instructionCount == 1 then
        sendToMaster 'disassemble' {
            command = pkg.command,
            seq = pkg.seq,
            success = true,
            body = { instructions = {} },
        }
        return
    end
    if not proto or not proto.code or #proto.code == 0 then
        sendToMaster 'disassemble' {
            command = pkg.command,
            seq = pkg.seq,
            success = false,
            message = "Cannot dump function",
        }
        return
    end
    local instructions = disassemble.disassemble_function(proto, luaver.LUAVERSION)
    local byteOffset = math.floor((pkg.offset or 0) / 4)
    local instOffset = defaultOffset + byteOffset + (pkg.instructionOffset or 0)
    local instCount = pkg.instructionCount or #instructions
    local result = {}
    local firstLocation = nil
    local srcId = nil
    if proto.source then
        local src = source.create(proto.source)
        if source.valid(src) then
            firstLocation = source.output(src)
            if src.sourceReference then
                srcId = tostring(src.sourceReference)
            elseif src.path then
                srcId = fs.path_native(fs.path_normalize(src.path))
            end
        end
    end
    local start = math.max(1, instOffset + 1)
    for i = start, math.min(start + instCount - 1, #instructions) do
        local inst = instructions[i]
        local r = {
            address = inst.address,
            instructionBytes = inst.instructionBytes,
            instruction = inst.opName .. "  " .. inst.operands,
        }
        if inst.line then
            r.line = inst.line
            r.column = 1
        end
        if inst.symbol then
            r.symbol = inst.symbol
        end
        if inst.presentationHint then
            r.presentationHint = inst.presentationHint
        end
        if firstLocation then
            r.location = firstLocation
            firstLocation = nil
        end
        if srcId and proto.linedefined and proto.lastlinedefined then
            r.instructionReference = ("bp_%d_%d_%d_%s"):format(
                proto.linedefined, proto.lastlinedefined, inst.pc, srcId
            )
        end
        result[#result + 1] = r
    end
    sendToMaster 'disassemble' {
        command = pkg.command,
        seq = pkg.seq,
        success = true,
        body = {
            instructions = result,
        },
    }
end

local function runLoop(reason, level)
    baseL = hookmgr.gethost()
    sendToMaster 'eventStop' (reason)
    skipFrame = level or 0
    workerThreadUpdate()
    while true do
        workerThreadUpdate(10)
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
    local bp = breakpoint.hit_bp(src, source.line(src, line))
    if bp then
        state = 'stopped'
        runLoop {
            reason = 'breakpoint',
            hitBreakpointIds = { bp.id }
        }
        return true
    end
end

local function debuggeeReady()
    while suspend do
        workerThreadUpdate(10)
    end
    if initialized then
        return true
    end
end

function event.bp(line, proto)
    if not debuggeeReady() then return end
    breakpoint.gate_instbreak(proto)
    rdebug.getinfo(0, "S", info)
    local src = source.create(info.source)
    event_breakpoint(src, line)
end

function event.funcbp(func)
    if not debuggeeReady() then return end
    local bp = breakpoint.hit_funcbp(func)
    if bp then
        state = 'stopped'
        runLoop {
            reason = 'function breakpoint',
            hitBreakpointIds = { bp.id }
        }
    end
end

function event.step(line, proto)
    if not debuggeeReady() then return end
    rdebug.getinfo(0, "S", info)
    local src = source.create(info.source)
    breakpoint.gate_instbreak(proto)
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
        runLoop {
            reason = stopReason
        }
    end
end

function event.newproto(proto, level)
    if not debuggeeReady() then return end
    rdebug.getinfo(level, "S", info)
    local src = source.create(info.source)
    if not source.valid(src) then
        return false
    end
    return breakpoint.newproto(proto, src, info.linedefined.."-"..info.lastlinedefined)
end

function event.update()
    debuggeeReady()
    workerThreadUpdate()
end

function event.instbp(proto)
    if not debuggeeReady() then return end
    if proto and rdebug.currentpc then
        local curpc = rdebug.currentpc(0)
        if curpc >= 0 then
            local bp = breakpoint.hit_instbp(proto, curpc)
            if bp then
                state = 'stopped'
                runLoop {
                    reason = 'instruction breakpoint',
                    hitBreakpointIds = { bp.id }
                }
            end
        end
    end
end

function event.autoUpdate(flag)
    autoUpdate = flag
    hookmgr.update_open(not noDebug and autoUpdate)
end

function event.print(...args)
    if not debuggeeReady() then return end
    local res = {}
    for i = 1, #args do
        res[#res + 1] = variables.tostring(args[i])
    end
    local str = table.concat(res, '\t')..'\n'
    rdebug.getinfo(1, "Sl", info)
    stdout(str, info)
    return true
end

function event.iowrite(...args)
    if not debuggeeReady() then return end
    local t = {}
    for i = 1, #args do
        t[#t + 1] = variables.tostring(args[i])
    end
    local res = table.concat(t, '\t')
    rdebug.getinfo(1, "Sl", info)
    stdout(res, info)
    return true
end

local ERREVENT_ERRRUN <const> = 0x02
local ERREVENT_ERRSYNTAX <const> = 0x03
local ERREVENT_ERRMEM <const> = 0x04
local ERREVENT_ERRERR <const> = 0x05
local ERREVENT_PANIC <const> = 0x10

local function GlobalFunction(name)
    return rdebug.fieldv(rdebug._G, name)
end

local function getExceptionType(errcode, skip)
    errcode = errcode & 0xF
    if errcode == ERREVENT_ERRRUN then
        if rdebug.getinfo(skip, "Slf", info) then
            if info.what ~= 'C' then
                return "runtime"
            end
            if rdebug.equal(info.func, GlobalFunction "assert") then
                return "assert"
            end
            if rdebug.equal(info.func, GlobalFunction "error") then
                return "error"
            end
        end
        return "other"
    elseif errcode == ERREVENT_ERRSYNTAX then
        return "syntax"
    elseif errcode == ERREVENT_ERRMEM then
        return "unknown"
    elseif errcode == ERREVENT_ERRERR then
        return "unknown"
    else
        return "unknown"
    end
end

local function getExceptionCaught(errcode, skip)
    if errcode & ERREVENT_PANIC ~= 0 then
        return "panic"
    end
    local pcall = GlobalFunction 'pcall'
    local xpcall = GlobalFunction 'xpcall'
    local level = skip
    while true do
        if not rdebug.getinfo(level, "f", info) then
            break
        end
        if level >= 100 then
            return 'native'
        end
        if rdebug.equal(info.func, pcall) then
            return 'lua'
        end
        if rdebug.equal(info.func, xpcall) then
            return 'lua'
        end
        level = level + 1
    end
    return 'native'
end

local function getExceptionFlags(errcode, skip)
    return {
        getExceptionType(errcode, skip),
        getExceptionCaught(errcode, skip),
    }
end

local function runException(flags, errobj)
    local errmsg = variables.tostring(errobj)
    local level, message, trace = traceback.traceback(flags, errmsg)
    if level < 0 then
        return
    end
    local bp = breakpoint.hitExceptionBreakpoint(flags, level, errobj)
    if not bp then
        return
    end
    currentException = {
        message = message,
        trace = trace,
    }
    state = 'stopped'
    runLoop({
        reason = 'exception',
        hitBreakpointIds = { bp.id },
        text = message,
    }, level)
end

function event.exception(errobj, errcode, level)
    if not debuggeeReady() then return end
    if errcode == nil or errcode == -1 then
        --TODO:暂时兼容旧版本
        errcode = ERREVENT_ERRRUN
    end
    local skip = level or 0
    runException(getExceptionFlags(errcode, skip), errobj)
end

function event.thread(co, type)
    if not debuggeeReady() then return end
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
    suspend = true
    debuggeeReady()
end

function event.setThreadName(name)
    sendToMaster 'setThreadName' (name)
end

function event.exit()
    sendToMaster 'exitWorker' {}
    channel.destroy(WorkerChannel)
end

hookmgr.init(function(name, ...)
    local ok, err = xpcall(function(...)
        if event[name] then
            return event[name](...)
        end
    end, debug.traceback, ...)
    if not ok then
        log.error("ERROR:"..tostring(err))
        return
    end
    return err
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
    hookmgr.update_open(not noDebug and autoUpdate)
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

sendToMaster 'initWorker' {}

hookmgr.update_open(true)
