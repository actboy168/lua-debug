local proto = require 'protocol'
local select = require 'select'

local function create(t)
    local m = {}
    local session
    local srvfd
    local write = ''
    local stat = {}
    function t.event(_, fd)
        session = fd
        select.send(session, write)
        write = ''
        select.close(srvfd)
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
    return m
end

return create
