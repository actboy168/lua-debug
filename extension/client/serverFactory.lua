
local ls = require 'bee.socket'
local proto = require 'protocol'
local fs = require 'bee.filesystem'
local inject = require 'inject'
local select = require 'select'

local function getunixpath(pid)
    local path = fs.exe_path():parent_path():parent_path():parent_path() / "windows" / "tmp" 
    fs.create_directories(path)
	return path / ("pid_%d.tmp"):format(pid)
end

local function server(t)
    local m = {}
    local session
    local srvfd
    local write = ''
    function t.event(_, fd)
        session = fd
        select.send(session, write)
        write = ''
        select.close(srvfd)
    end
    srvfd = assert(select.listen(t))
    function m.is_ready()
        return not not session
    end
    function m.is_closed()
        if not session then
            return false
        end
        return select.is_closed(session)
    end
    function m.send(pkg)
        if not session then
            write = write .. proto.send(pkg)
            return
        end
        select.send(session, proto.send(pkg))
    end
    local stat = {}
    function m.recv()
        if not session then
            return
        end
        return proto.recv(select.recv(session), stat)
    end
    return m
end

local function unix_server()
    local path = getunixpath(inject.current_pid())
    fs.remove(path)
    return server {
        protocol = 'unix',
        address = path:string()
    }, path
end


local function tcp_server(address, port)
    return server {
        protocol = 'tcp',
        address = address,
        port = port
    }
end

return {
    unix_server = unix_server,
    tcp_server = tcp_server,
}
