local nt = require "backend.master.named_thread"

return function (error_log, address)
    if not nt.init() then
        return
    end

    nt.createChannel "DbgMaster"

    if error_log then
        nt.createThread("error", package.path, package.cpath, ([[
            local err = thread.channel "errlog"
            local log = require "common.log"
            log.file = %q
            repeat
                local ok, msg = err:pop(0.05)
                if ok then
                    log.error("ERROR:" .. msg)
                end
            until MgrUpdate()
        ]]):format(error_log))
    end

    nt.createThread("master", package.path, package.cpath, ([[
        local dbg_io = require "common.io"(%s)
        local master = require "backend.master.mgr"
        master.init(dbg_io)
        repeat
            master.update()
        until MgrUpdate()
        select.closeall()
    ]]):format(address))
end
