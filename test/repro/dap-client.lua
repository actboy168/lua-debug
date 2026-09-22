-- 用一个假的 DAP 客户端复现：
--   目标 VM 主动加载调试器并监听端口 -> 关闭 autoUpdate（手动 update）-> 附加
--   -> 点“暂停” -> 在调试控制台执行“创建协程并立即执行”的表达式
--
-- 用法（在仓库根目录执行）：
--   luamake lua test/repro/dap-client.lua [pause|bp|step|stepbp|entry]
--   pause  : 用“暂停”进入停止状态（这个会复现问题）
--   entry  : 用 stopOnEntry 进入停止状态（同 pause，走的是同一条路径）
--   stepbp : 暂停后单步、单步撞上断点停下（另一条不经过 event.step 的停止路径）
--   bp     : 用“命中断点”进入停止状态（对照组，正常）
--   step   : 暂停后再单步（对照组，正常）
--
-- 必须用 release 构建的 publish/（luamake -mode release）：
--   debug 构建里 protected_area.h 的 check_recursive() 会让停止状态下
--   重入的 rdebug.* 调用直接报 "can't recursive"，反而掩盖这个 bug。
--
-- 环境变量：
--   REPRO_LUA          运行时目录名，默认 lua54（luajit / lua51..lua55）
--   REPRO_RUNTIME      publish/runtime 下的平台目录名，默认按当前平台推断
--   REPRO_TARGET_PORT  默认 55321
--   REPRO_ADAPTER_PORT 默认 55322
--   REPRO_AUTOUPDATE=1 不关目标进程的 autoUpdate（走调试器默认的 update hook）

local fs       = require "bee.filesystem"
local platform = require "bee.platform"
local bselect  = require "bee.select"
local socket   = require "bee.socket"
local sp       = require "bee.subprocess"
local thread   = require "bee.thread"
local time     = require "bee.time"

local MODES = {
    pause = true,
    bp = true,
    step = true,
    stepbp = true,
    entry = true,
}

local proto
local json

local function find_root()
    local p = arg and arg[0]
    if p then
        p = fs.absolute(fs.path(p)):lexically_normal()
        if p:filename():string() == "dap-client.lua" then
            return p:parent_path():parent_path():parent_path()
        end
    end
    return fs.current_path()
end

local ROOT          = find_root()
local PUBLISH       = ROOT / "publish"
local SRC_SCRIPT    = ROOT / "extension" / "script"
local PUB_SCRIPT    = PUBLISH / "script"
local TARGET_SCRIPT = ROOT / "test" / "repro" / "target.lua"
local WORKER_LOG    = PUBLISH / "worker.log"
local TARGET_LOG    = PUBLISH / "repro-target.log"
local ADAPTER_LOG   = PUBLISH / "repro-adapter.log"

assert(fs.exists(ROOT / "make.lua"), ("请在仓库根目录运行（当前 %s）"):format(ROOT:string()))

local EXE = platform.os == "windows" and ".exe" or ""
local SO  = platform.os == "windows" and ".dll" or ".so"

local function runtime_platform()
    local env = os.getenv("REPRO_RUNTIME")
    if env then
        return env
    end
    local arch = platform.Arch
    if platform.os == "windows" then
        if arch == "x86_64" then
            return "win32-x64"
        elseif arch == "x86" then
            return "win32-ia32"
        end
        return "win32-" .. arch
    elseif platform.os == "macos" then
        return arch == "arm64" and "darwin-arm64" or "darwin-x64"
    elseif platform.os == "linux" then
        return arch == "arm64" and "linux-arm64" or "linux-x64"
    end
    error(("不支持的平台 %s，请用 REPRO_RUNTIME 指定 publish/runtime 下的目录名"):format(platform.os))
end

local LUA_VER     = os.getenv("REPRO_LUA") or "lua54"
local RUNTIME     = PUBLISH / "runtime" / runtime_platform() / LUA_VER
local TARGET_EXE  = RUNTIME / ("lua" .. EXE)
local LUADEBUG    = RUNTIME / ("luadebug" .. SO)
local ADAPTER_EXE = PUBLISH / "bin" / ("lua-debug" .. EXE)

local TARGET_PORT  = tonumber(os.getenv("REPRO_TARGET_PORT") or 55321)
local ADAPTER_PORT = tonumber(os.getenv("REPRO_ADAPTER_PORT") or 55322)
local EVAL_EXPR    = "make_coroutine()"

package.path = ("%s/script/?.lua;%s"):format(PUBLISH:string(), package.path)

local function log(...)
    local n = select("#", ...)
    local parts = {}
    for i = 1, n do
        parts[i] = tostring((select(i, ...)))
    end
    print(table.concat(parts, " "))
end

local function fail(msg)
    error("FAIL: " .. msg, 0)
end

local function read_file(p)
    local f = io.open(p:string(), "rb")
    if not f then
        return nil
    end
    local data = f:read("a")
    f:close()
    return data
end

local function same_content(a, b)
    local da = read_file(a)
    return da ~= nil and da == read_file(b)
end

local function newest_mtime(dir, exts)
    local newest = 0
    for path, entry in fs.pairs_r(dir) do
        if entry:is_regular_file() then
            local name = path:filename():string()
            local hit = false
            for _, ext in ipairs(exts) do
                if name:sub(-#ext) == ext then
                    hit = true
                    break
                end
            end
            if hit then
                newest = math.max(newest, entry:last_write_time())
            end
        end
    end
    return newest
end

local function check_env()
    for _, f in ipairs {
        TARGET_EXE,
        LUADEBUG,
        ADAPTER_EXE,
        PUB_SCRIPT / "debugger.lua",
        PUB_SCRIPT / "common" / "json.lua",
        PUB_SCRIPT / "common" / "protocol.lua",
    } do
        if not fs.exists(f) then
            fail(("缺少 %s，请先 luamake -mode release 构建"):format(f:string()))
        end
    end
end

-- publish/script 是 luamake 从 extension/script 拷贝出来的（copy_extension）。
-- 这里保证它是当前源码，免得测到旧脚本。
local function sync_scripts()
    local copied = 0
    fs.create_directories(PUB_SCRIPT)
    for path, entry in fs.pairs_r(SRC_SCRIPT) do
        local dst = PUB_SCRIPT / fs.relative(path, SRC_SCRIPT)
        if entry:is_directory() then
            fs.create_directories(dst)
        else
            fs.create_directories(dst:parent_path())
            if not same_content(path, dst) then
                fs.copy_file(path, dst, fs.copy_options.overwrite_existing)
                copied = copied + 1
            end
        end
    end
    if copied > 0 then
        log(("[repro] 已同步 %d 个脚本到 publish/script（相当于 copy_extension）"):format(copied))
    end
end

local function check_binaries()
    -- last_write_time 是秒级，比 JS 的 mtimeMs 粗，够用
    local built = fs.last_write_time(LUADEBUG)
    local newer = {}
    for _, d in ipairs {
        ROOT / "src" / "luadebug",
        ROOT / "3rd" / "lua",
        ROOT / "3rd" / "bee.lua" / "binding",
    } do
        if fs.exists(d) and newest_mtime(d, { ".cpp", ".c", ".h" }) > built then
            newer[#newer + 1] = d
        end
    end
    if #newer > 0 then
        local names = {}
        for i, d in ipairs(newer) do
            names[i] = "          " .. d:string()
        end
        fail(("%s 比这些目录里的源码旧，请先 luamake -mode release 重新构建：\n%s\n          （debug 构建会掩盖这个问题，不能用来判断复现结果）")
            :format(LUADEBUG:string(), table.concat(names, "\n")))
    end
end

local function marker_line(marker)
    local text = assert(read_file(TARGET_SCRIPT), "can not read " .. TARGET_SCRIPT:string())
    local line = 0
    local pos = 1
    while true do
        local s, e = text:find("\n", pos, true)
        line = line + 1
        local l = s and text:sub(pos, s - 1) or text:sub(pos)
        if l:find(marker, 1, true) then
            return line
        end
        if not s then
            break
        end
        pos = e + 1
    end
    error("marker not found: " .. marker)
end

local function where(frame)
    if not frame or not frame.source then
        return "<unknown>"
    end
    return ("%s:%s"):format(frame.source.path or frame.source.name, tostring(frame.line))
end

local function connect(host, port, timeout)
    local deadline = time.monotonic() + (timeout or 10000)
    while true do
        local fd, err = socket.create("tcp")
        if not fd then
            error(err)
        end
        if fd:connect(host, port) == nil then
            fd:close()
        else
            local s = bselect.create()
            s:event_add(fd, bselect.SELECT_WRITE)
            s:wait(math.max(1, deadline - time.monotonic()))
            local connected = fd:status()
            s:close()
            if connected then
                return fd
            end
            fd:close()
        end
        if time.monotonic() > deadline then
            error(("can not connect %s:%d"):format(host, port))
        end
        thread.sleep(100)
    end
end

-- 停止事件本身不带位置，位置要从 stackTrace 拿（VS Code 也是这么做的）
local function locate(dap, threadId)
    local st = dap:request("stackTrace", { threadId = threadId, startFrame = 0, levels = 1 })
    return st.body.stackFrames[1]
end

local DAP = {}
DAP.__index = DAP

local function new_dap(fd)
    local self = setmetatable({
        fd = fd,
        stat = {},
        seq = 0,
        responses = {},
        names = {},
        events = {},
        stops = {},
        stop_cursor = 0,
        sel = bselect.create(),
    }, DAP)
    self.sel:event_add(fd, bselect.SELECT_READ)
    return self
end

function DAP:dispatch(msg)
    if msg.type == "response" then
        self.responses[msg.request_seq] = msg
        return
    end
    if msg.type ~= "event" then
        return
    end
    self.events[#self.events + 1] = msg
    if msg.event == "stopped" then
        self.stops[#self.stops + 1] = msg
    end
    if msg.event == "stopped" or msg.event == "thread"
        or msg.event == "initialized" or msg.event == "breakpoint" then
        log("  <- event " .. msg.event .. " " .. json.encode(msg.body or {}))
    end
end

-- 取出一条消息，超时返回 nil
function DAP:wait_message(timeout)
    local deadline = time.monotonic() + timeout
    while true do
        local msg = proto.recv("", self.stat)
        if msg then
            self:dispatch(msg)
            return msg
        end
        local remain = deadline - time.monotonic()
        if remain <= 0 then
            return nil
        end
        self.sel:wait(remain)
        local data = self.fd:recv()
        if data == nil then
            error("adapter 连接已断开")
        elseif data ~= false then
            -- 一次 recv 可能带回多条消息，多出来的留在 stat 里，下一轮开头继续取
            local got = proto.recv(data, self.stat)
            if got then
                self:dispatch(got)
                return got
            end
        end
    end
end

function DAP:send(data)
    while data ~= "" do
        local n = self.fd:send(data)
        if n == nil then
            error("adapter 连接已断开")
        elseif n == false then
            self.sel:event_mod(self.fd, bselect.SELECT_WRITE)
            self.sel:wait(5000)
            self.sel:event_mod(self.fd, bselect.SELECT_READ)
        else
            data = data:sub(n + 1)
        end
    end
end

function DAP:send_request(command, args)
    self.seq = self.seq + 1
    log("  -> " .. command)
    self.names[self.seq] = command
    self:send(proto.send({
        seq = self.seq,
        type = "request",
        command = command,
        arguments = args,
    }, self.stat))
    return self.seq
end

function DAP:wait_response(seq, timeout)
    local deadline = time.monotonic() + (timeout or 10000)
    while true do
        local msg = self.responses[seq]
        if msg then
            self.responses[seq] = nil
            if not msg.success then
                error(("%s failed: %s"):format(self.names[seq], msg.message))
            end
            return msg
        end
        local remain = deadline - time.monotonic()
        if remain <= 0 then
            error("timeout: " .. self.names[seq])
        end
        self:wait_message(remain)
    end
end

function DAP:request(command, args, timeout)
    return self:wait_response(self:send_request(command, args), timeout)
end

function DAP:wait_event(pred, timeout)
    local deadline = time.monotonic() + (timeout or 10000)
    while true do
        for _, msg in ipairs(self.events) do
            if pred(msg) then
                return msg
            end
        end
        local remain = deadline - time.monotonic()
        if remain <= 0 then
            error("timeout: wait event")
        end
        self:wait_message(remain)
    end
end

function DAP:wait_stopped(timeout)
    local deadline = time.monotonic() + (timeout or 10000)
    while true do
        -- 只看还没消费过的 stopped，避免拿历史事件当新事件
        if self.stops[self.stop_cursor + 1] then
            self.stop_cursor = self.stop_cursor + 1
            return self.stops[self.stop_cursor]
        end
        local remain = deadline - time.monotonic()
        if remain <= 0 then
            error("timeout: wait stopped")
        end
        self:wait_message(remain)
    end
end

local ctx = {}

local function dump(file, tag)
    local content = fs.exists(file) and read_file(file)
    if not content or content == "" then
        return
    end
    log(("--- %s (%s) ---"):format(file:string(), tag))
    io.write(content)
end

local function run(mode)
    check_env()
    sync_scripts()
    check_binaries()

    proto = require("common.protocol")
    json = require("common.json")

    local bp_line = marker_line("--@bp")
    local coroutine_line = marker_line("--@coroutine-first-line")
    log(("[repro] mode=%s bpLine=%d coroutineFirstLine=%d"):format(mode, bp_line, coroutine_line))

    fs.remove(WORKER_LOG)
    local target_log = assert(io.open(TARGET_LOG:string(), "wb"))
    local adapter_log = assert(io.open(ADAPTER_LOG:string(), "wb"))
    local env = {
        LUA_DEBUG_ROOT = PUBLISH:string(),
        LUA_DEBUG_CORE = LUADEBUG:string(),
    }
    -- Windows 上 spawn 默认不搜 PATH，这里全部给绝对路径，searchPath 只是保险
    ctx.target = assert(sp.spawn({
        TARGET_EXE,
        TARGET_SCRIPT,
        tostring(TARGET_PORT),
        cwd = RUNTIME,
        env = env,
        stdout = target_log,
        stderr = target_log,
        searchPath = true,
        hideWindow = true,
    }))
    ctx.adapter = assert(sp.spawn({
        ADAPTER_EXE,
        tostring(ADAPTER_PORT),
        cwd = PUBLISH / "bin",
        stdout = adapter_log,
        stderr = adapter_log,
        searchPath = true,
        hideWindow = true,
    }))

    local dap = new_dap(connect("127.0.0.1", ADAPTER_PORT))

    dap:request("initialize", {
        adapterID = "lua",
        clientID = "repro",
        linesStartAt1 = true,
        columnsStartAt1 = true,
        pathFormat = "path",
    })

    local attach = dap:send_request("attach", {
        type = "lua",
        request = "attach",
        name = "repro",
        address = ("127.0.0.1:%d"):format(TARGET_PORT),
        client = true, -- 同 VS Code 扩展侧: 由调试器去 connect 目标监听的端口
        workspaceFolder = ROOT:string(),
        luaVersion = LUA_VER,
        stopOnEntry = mode == "entry",
        stopOnThreadEntry = false,
    })
    dap:wait_event(function (msg) return msg.event == "initialized" end, 15000)
    dap:wait_response(attach)
    log("[repro] attached")

    dap:request("configurationDone", {})
    dap:wait_event(function (msg) return msg.event == "thread" end, 10000)
    log("[repro] target thread started")

    local stop
    if mode == "bp" then
        local res = dap:request("setBreakpoints", {
            source = { path = TARGET_SCRIPT:string() },
            breakpoints = { { line = bp_line } },
            sourceModified = false,
        })
        log("[repro] breakpoint: " .. json.encode(res.body and res.body.breakpoints or {}))
        stop = dap:wait_stopped(15000)
        log(("[repro] 断点在 %s (reason=%s)"):format(where(locate(dap, stop.body.threadId)), stop.body.reason))
    elseif mode == "stepbp" then
        -- 先暂停，再在停止状态下加断点，然后 stepIn 直到踩到断点行。
        -- 这个停止由 event_breakpoint 直接 runLoop，不经过 event.step 的取消分支，
        -- 此时 step_in 的 stepL==0 仍然挂着。
        dap:request("pause", { threadId = 1 })
        stop = dap:wait_stopped(15000)
        log(("[repro] 暂停在 %s (reason=%s)"):format(where(locate(dap, stop.body.threadId)), stop.body.reason))
        local res = dap:request("setBreakpoints", {
            source = { path = TARGET_SCRIPT:string() },
            breakpoints = { { line = bp_line } },
            sourceModified = false,
        })
        log("[repro] breakpoint: " .. json.encode(res.body and res.body.breakpoints or {}))
        for i = 1, 10 do
            if stop.body.reason == "breakpoint" then
                break
            end
            dap:request("stepIn", { threadId = stop.body.threadId })
            stop = dap:wait_stopped(15000)
            log(("[repro] stepIn(%d) 停在 %s (reason=%s)")
                :format(i, where(locate(dap, stop.body.threadId)), stop.body.reason))
        end
        if stop.body.reason ~= "breakpoint" then
            fail("stepIn 10 次都没踩到断点，无法构造出“单步被断点打断”的停止状态")
        end
    elseif mode == "entry" then
        stop = dap:wait_stopped(15000)
        log(("[repro] 入口停在 %s (reason=%s)"):format(where(locate(dap, stop.body.threadId)), stop.body.reason))
    else
        dap:request("pause", { threadId = 1 })
        stop = dap:wait_stopped(15000)
        log(("[repro] 暂停在 %s (reason=%s)"):format(where(locate(dap, stop.body.threadId)), stop.body.reason))
        if mode == "step" then
            dap:request("next", { threadId = stop.body.threadId })
            stop = dap:wait_stopped(15000)
            log(("[repro] 单步停在 %s (reason=%s)"):format(where(locate(dap, stop.body.threadId)), stop.body.reason))
        end
    end

    local thread_id = stop.body.threadId
    local st = dap:request("stackTrace", { threadId = thread_id, startFrame = 0, levels = 1 })
    local frame = st.body.stackFrames[1]
    log(("[repro] 停止位置 %s frameId=%s"):format(where(frame), tostring(frame.id)))

    log("[repro] 在调试控制台执行: " .. EVAL_EXPR)
    local evaluate = dap:send_request("evaluate", {
        expression = EVAL_EXPR,
        frameId = frame.id,
        context = "repl",
    })

    -- evaluate 还没返回之前，是否又收到了 stopped
    local deadline = time.monotonic() + 5000
    while not dap.responses[evaluate] and not dap.stops[dap.stop_cursor + 1] do
        local remain = deadline - time.monotonic()
        if remain <= 0 then
            break
        end
        dap:wait_message(remain)
    end
    local second_stop = dap.stops[dap.stop_cursor + 1] ~= nil

    log("")
    log("===== 结论 =====")
    if second_stop then
        -- evaluate 还没返回就又收到 stopped：不管停在哪一行，都说明停止期间又被误触发了一次停止
        local frames = dap:request("stackTrace", { threadId = thread_id, startFrame = 0, levels = 3 })
        log("[repro] evaluate 还没返回，调试器却又停在:")
        for _, f in ipairs(frames.body.stackFrames) do
            log(("           %s  %s"):format(where(f), f.name))
        end
        log("[repro] 发送 continue，看 evaluate 是否马上返回...")
        dap:request("continue", { threadId = thread_id, allThreadsContinued = true })
        local ok, res = pcall(dap.wait_response, dap, evaluate, 60000)
        if ok then
            log("[repro] continue 之后 evaluate 返回: " .. json.encode(res.body.result))
        else
            log("[repro] continue 之后 evaluate 失败: " .. tostring(res))
        end
        log("BUG 已复现：停止状态下执行“创建协程并立即执行”，evaluate 返回前又收到一次 stopped，调试控制台的表达式被阻塞")
        log(("（第二次 stopped 在 target.lua:%s，协程第一行是 target.lua:%d，仅作诊断）")
            :format(tostring((frames.body.stackFrames[1] or {}).line), coroutine_line))
        return false
    end

    local ok, res = pcall(dap.wait_response, dap, evaluate, 60000)
    if ok then
        log("[repro] BUG 未复现：evaluate 正常返回 " .. json.encode(res.body.result) .. "（没有多余的停止）")
        return true
    end
    log("[repro] 意外：evaluate 失败 " .. tostring(res))
    return false
end

local function cleanup()
    if ctx.target then
        pcall(ctx.target.kill, ctx.target)
    end
    if ctx.adapter then
        pcall(ctx.adapter.kill, ctx.adapter)
    end
    thread.sleep(200)
end

local function main()
    local mode = (arg and arg[1] or os.getenv("REPRO_MODE") or "pause"):gsub("^%-%-mode=", "")
    if not MODES[mode] then
        io.stderr:write("usage: luamake lua test/repro/dap-client.lua [pause|bp|step|stepbp|entry]\n")
        os.exit(2)
    end

    local ok, res = pcall(run, mode)
    cleanup()
    dump(WORKER_LOG, "worker")
    dump(TARGET_LOG, "target")
    dump(ADAPTER_LOG, "adapter")
    if not ok then
        io.stderr:write(tostring(res), "\n")
        os.exit(2)
    end
    os.exit(res and 0 or 1)
end

main()
