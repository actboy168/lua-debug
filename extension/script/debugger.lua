local root = ...

if debug.getregistry()["lua-debug"] then
    local dbg = debug.getregistry()["lua-debug"]
    local empty = {root = dbg.root}
    function empty:init()
        return self
    end
    function empty:start()
        return self
    end
    function empty:event(...)
        return self
    end
    function empty:set_wait(...)
        return self
    end
    function empty:setup_patch()
        return self
    end
    return empty
end

local function detectLuaDebugPath(cfg)
    local OS
    local ARCH
    do
        local function shell(command)
            --NOTICE: io.popen可能会多线程不安全
            local f = assert(io.popen(command, 'r'))
            local r = f:read '*l'
            f:close()
            return r:lower()
        end
        local function detect_windows()
            OS = "windows"
            if os.getenv "PROCESSOR_ARCHITECTURE" == "AMD64" then
                ARCH = "x86_64"
            else
                ARCH = "x86"
            end
        end
        local function detect_linux()
            OS = "linux"
            ARCH = "x86_64"
            local machine = shell "uname -m"
            assert(machine:match "x86_64" or machine:match "amd64", "unknown ARCH")
        end
        local function detect_macos()
            OS = "macos"
            ARCH = shell "uname -m"
            assert(ARCH == "x86_64" or ARCH == "arm64", "unknown ARCH")
        end
        if package.config:sub(1,1) == "\\" then
            detect_windows()
        else
            local name = shell 'uname -s'
            if name == "linux" then
                detect_linux()
            elseif name == "darwin" then
                detect_macos()
            else
                error "unknown OS"
            end
        end
    end

    local rt = "/runtime/" .. OS .. "/" .. ARCH
    if cfg.latest then
        rt = rt .. "/lua-latest"
    elseif _VERSION == "Lua 5.4" then
        rt = rt .. "/lua54"
    elseif _VERSION == "Lua 5.3" then
        rt = rt .. "/lua53"
    elseif _VERSION == "Lua 5.2" then
        rt = rt .. "/lua52"
    elseif _VERSION == "Lua 5.1" then
        rt = rt .. "/lua51"
    else
        error(_VERSION .. " is not supported.")
    end

    local ext = OS == "windows" and "dll" or "so"
    return root..rt..'/remotedebug.'..ext
end

local function initDebugger(dbg, cfg)
    if type(cfg) == "string" then
        cfg = {address = cfg}
    end

    local remotedebug = os.getenv "LUA_DEBUG_PATH"
    local updateenv = false
    if not remotedebug then
        remotedebug = detectLuaDebugPath(cfg)
        updateenv = true
    end
    local isWindows = package.config:sub(1,1) == "\\"
    if isWindows then
        assert(package.loadlib(remotedebug,'init'))(cfg.luaapi)
    end

    ---@type RemoteDebug
    dbg.rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()
    if updateenv then
        dbg.rdebug.setenv("LUA_DEBUG_PATH",remotedebug)
    end

    local function utf8(s)
        if cfg.ansi and isWindows then
            return dbg.rdebug.a2u(s)
        end
        return s
    end
    dbg.root = utf8(root)
    dbg.address = cfg.address and utf8(cfg.address) or nil
end

local dbg = {}

function dbg:start(cfg)
    initDebugger(self, cfg)

    local bootstrap_lua = ([[
        package.path = %q
        require "remotedebug.thread".bootstrap_lua = debug.getinfo(1, "S").source
    ]]):format(
          self.root..'/script/?.lua'
    )
    self.rdebug.start(("assert(load(%q))(...)"):format(bootstrap_lua) .. ([[
        local logpath = %q
        local log = require 'common.log'
        log.file = logpath..'/worker.log'
        require 'backend.master' .init(logpath, %q, true)
        require 'backend.worker'
    ]]):format(
          self.root
        , ("%q, %s"):format(dbg.address, cfg.client == true and "true" or "false")
    ))
    return self
end

function dbg:attach(cfg)
    initDebugger(self, cfg)

    local bootstrap_lua = ([[
        package.path = %q
        require "remotedebug.thread".bootstrap_lua = debug.getinfo(1, "S").source
    ]]):format(
          self.root..'/script/?.lua'
    )
    self.rdebug.start(("assert(load(%q))(...)"):format(bootstrap_lua) .. ([[
        if require 'backend.master' .has() then
            local logpath = %q
            local log = require 'common.log'
            log.file = logpath..'/worker.log'
            require 'backend.worker'
        end
    ]]):format(
        self.root
    ))
    return self
end

function dbg:event(...)
    self.rdebug.event(...)
    return self
end

function dbg:set_wait(name, f)
    _G[name] = function(...)
        _G[name] = nil
        f(...)
        self:event 'wait'
    end
    return self
end

function dbg:setup_patch()
    local rawxpcall = xpcall
    function pcall(f, ...)
        return rawxpcall(f,
            function(msg)
                self:event("exception", msg)
                return msg
            end,
        ...)
    end
    function xpcall(f, msgh, ...)
        return rawxpcall(f,
            function(msg)
                self:event("exception", msg)
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
        return function(...)
            self:event("thread", co, 0)
            return coreturn(co, wf(...))
        end
    end
    return self
end

debug.getregistry()["lua-debug"] = dbg

return dbg
