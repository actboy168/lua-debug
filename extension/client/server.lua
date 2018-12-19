
local ls = require 'bee.socket'
local proto = require 'protocol'
local socket = require 'socket'

local function tcpchannel(proxy, fd)
    local stat = {}
    socket.init(fd, function()
        while true do
            local pkg = proto.recv(fd:recv() or '', stat)
            if pkg then
                proxy.recv(pkg)
            else
                break
            end
        end
    end)
    local m = {}
    function m.send(pkg)
        socket.send(fd, proto.send(pkg))
    end
    return m
end

local function tcp_client(proxy, ip, port)
    local fd = assert(ls.connect(ip, port))
    return tcpchannel(proxy, fd)
end

local function tcp_server(proxy, ip)
    local port = 11000
    local socket
    repeat
        port = port + 1
        socket = assert(ls.bind("tcp", ip, port))
    until socket
    
    local listen = {}
    function listen:accept(timeout)
        if not ls.select({socket}, timeout) then
            return
        end
        local fd = socket:accept()
        return tcpchannel(proxy, fd)
    end
    return listen, port
end

return {
    tcp_client = tcp_client,
    tcp_server = tcp_server,
}
