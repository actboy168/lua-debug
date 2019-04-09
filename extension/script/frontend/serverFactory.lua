local proto = require 'protocol'
local select = require 'frontend.select'

local function create(t)
    local m = {}
    local session
    local srvfd
    local write = ''
    local stat = {}
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
        stat.debug = v
    end
    function m.send(pkg)
        if not session then
            write = write .. proto.send(pkg, stat)
            return
        end
        select.send(session, proto.send(pkg, stat))
    end
    function m.recv()
        if not session then
            return
        end
        return proto.recv(select.recv(session), stat)
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
