local net = require 'common.net'
local proto = require 'common.protocol'

local function parseAddress(param)
    local mode, address = param:match "^([a-z]+):(.*)"
    if address:sub(1,1) == '@' then
        local fs = require "bee.filesystem"
        address = address:gsub("%$tmp", (fs.temp_directory_path():string():gsub("([/\\])$", "")))
        return {
            protocol = 'unix',
            address = address:sub(2),
            mode = mode,
        }
    end
    local ipv4, port = address:match("(%d+%.%d+%.%d+%.%d+):(%d+)")
    if ipv4 then
        return {
            protocol = 'tcp',
            address = ipv4,
            port = tonumber(port),
            mode = mode,
        }
    end
    local ipv6, port = address:match("%[([%d:a-fA-F]+)%]:(%d+)")
    if ipv6 then
        return {
            protocol = 'tcp6',
            address = ipv6,
            port = tonumber(port),
            mode = mode,
        }
    end
    error "Invalid address."
end

return function (param)
    local t = parseAddress(param)
    local stat = {}
    local readbuf = ''
    local writebuf = ''
    local server = nil
    local session = nil
    local closefunc = function () end
    local m = {}
    local function cleanup()
        session = nil
        readbuf = ''
        writebuf = ''
        stat = { debug = stat.debug }
        closefunc()
    end
    local function init_session(s)
        if session then
            return
        end
        session = s
        function session:on_data(data)
            readbuf = readbuf .. data
        end
        function session:on_close()
            cleanup()
        end
        return true
    end
    if t.mode == "connect" then
        local function try_connect()
            local s = net.connect(t.protocol, t.address, t.port)
            if s then
                function s:on_connected()
                    local ok = init_session(s)
                    assert(ok)
                    if writebuf ~= '' then
                        s:write(writebuf)
                        writebuf = ''
                    end
                end
                function s:on_error()
                    try_connect()
                end
            else
                net.async(try_connect)
            end
        end
        try_connect()
    elseif t.mode == "listen" then
        server = assert(net.listen(t.protocol, t.address, t.port))
        function server:on_accepted(new_s)
            return init_session(new_s)
        end
    else
        return
    end
    function m.event_close(f)
        closefunc = f
    end
    function m.debug(v)
        stat.debug = v
    end
    function m.sendmsg(pkg)
        local data = proto.send(pkg, stat)
        if session == nil then
            writebuf = writebuf .. data
            return
        end
        session:write(data)
    end
    function m.recvmsg()
        local data = readbuf
        readbuf = ''
        return proto.recv(data, stat)
    end
    function m.update(timeout)
        net.update(timeout)
    end
    function m.closeall()
        local fds = {}
        if t.mode == "connect" then
            if session == nil then
                return
            end
            session:close()
            fds[1] = session
        elseif t.mode == "listen" then
			fds[1] = server
			if session ~= nil then
				session:close()
				fds[2] = session
			end
			server:close()
        end
        local function is_finish()
            for _, fd in ipairs(fds) do
                if not fd:is_closed() then
                    return false
                end
            end
            return true
        end
        while not is_finish() do
            net.update(0)
        end
    end
    return m
end
