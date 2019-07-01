local network = require "common.network"

return function (address, client)
    local server = network.open(address, client)
    local m = {}
    function m:event_in(f)
        self.fsend = f
    end
    function m:event_close(f)
        self.fclose = f
    end
    function m:update()
        network.update(0.05)
        local data = server.recvRaw()
        if data ~= '' then
            self.fsend(data)
        end
        return true
    end
    function m:send(data)
        server.sendRaw(data)
    end
    function m:close()
        server.close()
        self.fclose()
    end
    return m
end
