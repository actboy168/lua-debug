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

local dbg = {}

function dbg:init(cfg)
    local platform = (function()
        if package.config:sub(1,1) == "\\" then
            return "windows"
        end
        local name = io.popen('uname -s','r'):read('*l')
        if name == "Linux" then
            return "linux"
        elseif name == "Darwin" then
            return "macos"
        end
        error "unknown platform"
    end)()

    local arch = (function()
        if string.packsize then
            local size = string.packsize "T"
            if size == 8 then
                return 64
            end
            if size == 4 then
                return 32
            end
        else
            if platform ~= "windows" then
                return 64
            end
            local pointer = tostring(io.stderr):match "%((.*)%)"
            if pointer then
                if #pointer <= 10 then
                    return 32
                end
                return 64
            end
        end
        assert(false, "unknown arch")
    end)()

    local rt = "/runtime"
    if platform == "windows" then
        if arch == 64 then
            rt = rt .. "/win64"
        else
            rt = rt .. "/win32"
        end
    else
        assert(arch == 64)
        rt = rt .. "/" .. platform
    end
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

    local ext = platform == "windows" and "dll" or "so"
    local remotedebug = cfg.root..rt..'/remotedebug.'..ext
    if cfg.luaapi then
        assert(package.loadlib(remotedebug,'init'))(cfg.luaapi)
    end

    ---@type RemoteDebug
    self.rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()

    local function utf8(s)
        if cfg.ansi and platform == "windows" then
            return self.rdebug.a2u(s)
        end
        return s
    end
    self.root = utf8(cfg.root)
    self.utf8 = utf8
    self.path  = self.root..'/script/?.lua'
    self.cpath = self.root..rt..'/?.'..ext
end

function dbg:start(addr, client)
    local address = ("%q, %s"):format(self.utf8(addr), client == true and "true" or "false")
    local bootstrap_lua = ([[
        package.path = %q
        package.cpath = %q
        require "remotedebug.thread".bootstrap_lua = debug.getinfo(1, "S").source
    ]]):format(
          self.path
        , self.cpath
    )
    self.rdebug.start(("assert(load(%q))(...)"):format(bootstrap_lua) .. ([[
        local logpath = %q
        local log = require 'common.log'
        log.file = logpath..'/worker.log'
        require 'backend.master' (logpath, %q, true)
        require 'backend.worker'
    ]]):format(
          self.root
        , address
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
