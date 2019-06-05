local proto = require 'common.protocol'
local select = require 'common.select'

local function create(t)
    local m = {}
    local session
    local srvfd
    local write = ''
    local statR = {}
    local statW = {}
    function t.event(status, fd)
        if status == 'failed' then
            assert(t.client)
            select.close(srvfd)
            srvfd = assert(select.connect(t))
            return
        end
        session = fd
        select.send(session, write)
        write = ''
        if not t.client then
            select.close(srvfd)
        end
    end
    if t.client then
        srvfd = assert(select.connect(t))
    else
        srvfd = assert(select.listen(t))
    end
    function m.debug(v)
        statR.debug = v
        statW.debug = v
    end
    function m.sendRaw(data)
        if not session then
            write = write .. data
            return
        end
        select.send(session, data)
    end
    function m.recvRaw()
        if not session then
            return ''
        end
        return select.recv(session)
    end
    function m.send(pkg)
        m.sendRaw(proto.send(pkg, statW))
    end
    function m.recv()
        return proto.recv(m.recvRaw(), statR)
    end
    function m.is_closed()
        if not session then
            return false
        end
        return select.is_closed(session)
    end
    return m
end

return create
