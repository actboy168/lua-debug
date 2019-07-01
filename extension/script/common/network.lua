local proto = require 'common.protocol'
local select = require 'common.select'

local function parseAddress(address, client)
    if address:sub(1,1) == '@' then
        return {
            protocol = 'unix',
            address = address:sub(2),
            client = client,
        }
    end
    local ipv4, port = address:match("(%d+%.%d+%.%d+%.%d+):(%d+)")
    if ipv4 then
        return {
            protocol = 'tcp',
            address = ipv4,
            port = tonumber(port),
            client = client,
        }
    end
    local ipv6, port = address:match("%[([%d:a-fA-F]+)%]:(%d+)")
    if ipv6 then
        return {
            protocol = 'tcp',
            address = ipv6,
            port = tonumber(port),
            client = client,
        }
    end
    error "Invalid address."
end

local function open(address, client)
    local t = parseAddress(address, client)
    local m = {}
    local session
    local srvfd
    local write = ''
    local statR = {}
    local statW = {}
    function t.event(status, fd)
        if status == 'connect start' then
            assert(t.client)
            srvfd = fd
            return
        end
        if status == 'connect failed' then
            assert(t.client)
            select.close(srvfd)
            select.wantconnect(t)
            return
        end
        if session then
            fd:close()
            return
        end
        session = fd
        select.send(session, write)
        write = ''
    end
    if t.client then
        select.wantconnect(t)
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
    function m.close()
        session = nil
        write = ''
    end
    function m.is_closed()
        if not session then
            return false
        end
        return select.is_closed(session)
    end
    return m
end

return {
    open = open,
    update = select.update,
}
