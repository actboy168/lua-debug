local theadpool = {}

return function (path, cpath, errlog, addr)
    local thread = require "remotedebug.thread"
    local ok, err = pcall(thread.newchannel, "DbgMaster")
    if not ok then
        if err:sub(1,17) == "Duplicate channel" then
            return
        end
        error(err)
    end

    thread.newchannel "errorExit"
    thread.newchannel "masterExit"

    theadpool["error"] = thread.thread (([[
        package.path = %q
        package.cpath = %q
        local thread = require "remotedebug.thread"
        local err = thread.channel "errlog"
        local exit = thread.channel "errorExit"
        local log = require "common.log"
        log.file = %q
        repeat
            local ok, msg = err:pop(0.05)
            if ok then
                log.error("ERROR:" .. msg)
            end
        until exit:pop()
    ]]):format(path, cpath, errlog))

    local bootstrap = ([=[
        package.path = %q
        package.cpath = %q
        local addr = %q

        local function split(str)
            local r = {}
            str:gsub('[^:]+', function (w) r[#r+1] = w end)
            return r
        end
        local l = split(addr)
        local t = {}
        if #l <= 1 then
            t.protocol = 'tcp'
            t.address = addr
            t.port = 4278
        else
            if l[1] == 'pipe' then
                t.protocol = 'unix'
                t.address = addr:sub(6)
                t.client = true
            elseif l[1] == 'listen' then
                t.protocol = 'tcp'
                if #l == 2 then
                    t.address = l[2]
                    t.port = 4278
                else
                    t.address = l[2]
                    t.port = tonumber(l[3])
                end
            else
                t.protocol = 'tcp'
                t.address = l[1]
                t.port = tonumber(l[2])
            end
        end
        if t.address == "localhost" then
            t.address = "127.0.0.1"
        end

        local serverFactory = require "common.serverFactory"
        local server = serverFactory(t)

        local dbg_io = {}
        function dbg_io:event_in(f)
            self.fsend = f
        end
        function dbg_io:event_close(f)
            self.fclose = f
        end
        function dbg_io:update()
            local data = server.recvRaw()
            if data ~= '' then
                self.fsend(data)
            end
            return true
        end
        function dbg_io:send(data)
            server.sendRaw(data)
        end
        function dbg_io:close()
            self.fclose()
        end

        local thread = require "remotedebug.thread"
        local exit = thread.channel "masterExit"
        local select = require "common.select"
        local master = require 'backend.master'
        master.init(dbg_io)
        repeat
            select.update(0.05)
            master.update()
        until exit:pop()
        select.closeall()
    ]=]):format(path, cpath, addr)
    theadpool["master"] = thread.thread(bootstrap)

    setmetatable(theadpool, {__gc=function(self)
        for _, thd in ipairs {"master","error"} do
            local chan = thread.channel(thd.."Exit")
            chan:push "EXIT"
            self[thd]:wait()
        end
    end})
end
