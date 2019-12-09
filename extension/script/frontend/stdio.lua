local subprocess = require 'bee.subprocess'
local proto = require 'common.protocol'
local STDIN = io.stdin
local STDOUT = io.stdout
local peek = subprocess.peek
subprocess.filemode(STDIN, 'b')
subprocess.filemode(STDOUT, 'b')
STDIN:setvbuf 'no'
STDOUT:setvbuf 'no'

local m = {}
local stat = {}
function m.debug(v)
    stat.debug = v
end
function m.sendmsg(pkg)
    STDOUT:write(proto.send(pkg, stat))
end
function m.recvmsg()
    local n = peek(STDIN)
    if n == nil or n == 0 then
        return proto.recv('', stat)
    end
    return proto.recv(STDIN:read(n), stat)
end

return m
