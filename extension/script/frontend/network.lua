local net = require "common.network"
local proto = require 'common.protocol'

return function (address, client)
    local m = net(address, client)
    local statR = {}
    local statW = {}
    function m.debug(v)
        statR.debug = v
        statW.debug = v
    end
    function m.sendmsg(pkg)
        m.send(proto.send(pkg, statW))
    end
    function m.recvmsg()
        return proto.recv(m.recv(), statR)
    end
    return m
end
