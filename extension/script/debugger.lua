local selfsource = ...

if not selfsource then
    local source = debug.getinfo(1, "S").source
    if source:sub(1, 1) == "@" then
        local filepath = source:sub(2)
        selfsource = filepath
    end
end

local IsWindows = package.config:sub(1, 1) == "\\"
local root = selfsource
    :match "(.+)[/][^/]+$"
    :match "(.+)[/][^/]+$"

if debug.getregistry()["lua-debug"] then
    local dbg = debug.getregistry()["lua-debug"]
    local empty = { root = dbg.root }
    function empty:init()
        return self
    end

    function empty:start()
        return self
    end

    function empty:attach()
        return self
    end

    function empty:event(what, ...)
        if what == "setThreadName" then
            dbg:event(what, ...)
        end
        return self
    end

    function empty:set_wait()
        return self
    end

    function empty:setup_patch()
        return self
    end

    return empty
end

local function detectLuaDebugPath(cfg)
    local PLATFORM = cfg.platform or os.getenv "LUA_DEBUG_PLATFORM"
    if not PLATFORM then
        local function shell(command)
            --NOTICE: io.popen可能会多线程不安全
            local f = assert(io.popen(command, 'r'))
            local r = f:read '*l'
            f:close()
            return r:lower()
        end
        local function detect_windows()
            if os.getenv "PROCESSOR_ARCHITECTURE" == "AMD64" then
                PLATFORM = "win32-x64"
            else
                PLATFORM = "win32-ia32"
            end
        end
        local function detect_linux()
            local machine = shell "uname -m"
            if machine == "x86_64" or machine == "amd64" then
                PLATFORM = "linux-x64"
            elseif machine == "aarch64" then
                PLATFORM = "linux-arm64"
            else
                error "unknown ARCH"
            end
        end
        local function detect_android()
            PLATFORM = "linux-arm64"
        end
        local function detect_macos()
            if shell "uname -m" == "arm64" then
                PLATFORM = "darwin-arm64"
            else
                PLATFORM = "darwin-x64"
            end
        end
        local function detect_bsd()
            local machine = shell "uname -m"
            if machine == "x86_64" or machine == "amd64" then
                PLATFORM = "bsd-x64"
            else
                error "unknown ARCH"
            end
        end
        if IsWindows then
            detect_windows()
        else
            local name = shell 'uname -s'
            if name == "linux" then
                if shell 'uname -o' == 'android' then
                    detect_android()
                else
                    detect_linux()
                end
            elseif name == "darwin" then
                detect_macos()
            elseif name == "netbsd" or name == "freebsd" then
                detect_bsd()
            else
                error "unknown OS"
            end
        end
    end

    local rt = "/runtime/"..PLATFORM
    if cfg.luaVersion then
        rt = rt.."/"..cfg.luaVersion
    elseif _VERSION == "Lua 5.4" then
        rt = rt.."/lua54"
    elseif _VERSION == "Lua 5.3" then
        rt = rt.."/lua53"
    elseif _VERSION == "Lua 5.2" then
        rt = rt.."/lua52"
    elseif _VERSION == "Lua 5.1" then
        if (tostring(assert):match('builtin') ~= nil) then
            rt = rt.."/luajit"
            jit.off()
        else
            rt = rt.."/lua51"
        end
    else
        error(_VERSION.." is not supported.")
    end

    local ext = IsWindows and "dll" or "so"
    return root..rt..'/luadebug.'..ext
end

---@param cfg luadebugger.config
local function initDebugger(dbg, cfg)
    if type(cfg) == "string" then
        cfg = { address = cfg }
    end

    local key_core = cfg.debug_debugger and "LUA_DEBUG_CORE_DEBUGGER" or "LUA_DEBUG_CORE"..(cfg.tag or '')
    local key_path = cfg.debug_debugger and "LUA_DEBUG_PATH_DEBUGGER" or "LUA_DEBUG_PATH"..(cfg.tag or '')

    local luadebug = os.getenv (key_core)
    local updateenv = false
    if not luadebug then
        luadebug = detectLuaDebugPath(cfg)
        updateenv = true
    end
    if IsWindows then
        if cfg.debug_debugger then
            package.loadlib(luadebug, 'setflag_debugself')()
        end
        assert(package.loadlib(luadebug, 'init'))(cfg.luaapi)
    end

    ---@type LuaDebug
    dbg.rdebug = assert(package.loadlib(luadebug, 'luaopen_luadebug'))()
    if not os.getenv(key_path) then
        dbg.rdebug.setenv(key_path, selfsource)
    end
    if updateenv then
        dbg.rdebug.setenv(key_core, luadebug)
    end

    local function utf8(s)
        if cfg.ansi and IsWindows then
            return dbg.rdebug.a2u(s)
        end
        return s
    end
    dbg.root = utf8(root)
    dbg.address = cfg.address and utf8(cfg.address) or nil
end

---@class luadebugger.config
---@field address string
---@field client? boolean
---@field luaapi? string notice: buffer of FindLuaApi*
---@field luaVersion? 'lua51' | 'lua52' | 'lua53' | 'lua54' | 'luajit' | 'lua-latest'
---@field ansi? boolean
---@field tag? string channel name tag for multi lua vm
---@field debug_debugger? true for debug debugger
---@field debug_debugger_master? true
---@field debug_debugger_worker? true

local dbg = {}

---@param cfg luadebugger.config
function dbg:start(cfg)
    initDebugger(self, cfg)

    self.rdebug.start(([[
        local rootpath = %q
        package.path = rootpath.."/script/?.lua"
        require "backend.bootstrap". start(rootpath, %q..%q, %q, %s)
    ]]):format(
        self.root,
        cfg.client == true and "connect:" or "listen:",
        dbg.address,
        cfg.tag or '',
        cfg.debug_debugger_master and 'true' or 'false'
    ))
    return self
end

---@param cfg luadebugger.config
function dbg:attach(cfg)
    initDebugger(self, cfg or {})

    self.rdebug.start(([[
        local rootpath = %q
        package.path = rootpath..'/script/?.lua'
        require 'backend.bootstrap'. attach(rootpath, %q)
    ]]):format(
        self.root,
        cfg.tag or ''
    ))
    return self
end

function dbg:stop()
    self.rdebug.clear()
end

function dbg:event(...)
    self.rdebug.event(...)
    return self
end

function dbg:set_wait(name, f)
    _G[name] = function (...)
        _G[name] = nil
        f(...)
        self:event 'wait'
    end
    return self
end

function dbg:setup_patch()
    local ERREVENT_ERRRUN = 0x02
    local rawxpcall = xpcall
    function pcall(f, ...)
        return rawxpcall(f,
            function (msg)
                self:event("exception", msg, ERREVENT_ERRRUN, 3)
                return msg
            end,
            ...)
    end

    function xpcall(f, msgh, ...)
        return rawxpcall(f,
            function (msg)
                self:event("exception", msg, ERREVENT_ERRRUN, 3)
                return msgh and msgh(msg) or msg
            end
            , ...)
    end

    local rawcoroutineresume = coroutine.resume
    local rawcoroutinewrap   = coroutine.wrap
    local function coreturn(co, ...)
        self:event("thread", co, 1)
        return ...
    end
    function coroutine.resume(co, ...)
        self:event("thread", co, 0)
        return coreturn(co, rawcoroutineresume(co, ...))
    end

    function coroutine.wrap(f)
        local wf = rawcoroutinewrap(f)
        local _, co = debug.getupvalue(wf, 1)
        return function (...)
            self:event("thread", co, 0)
            return coreturn(co, wf(...))
        end
    end

    return self
end

debug.getregistry()["lua-debug"] = dbg

return dbg
