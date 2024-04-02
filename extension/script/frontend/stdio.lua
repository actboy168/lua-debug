local subprocess = require 'bee.subprocess'
local platform = require 'bee.platform'
local proto = require 'common.protocol'
local STDIN = io.stdin
local STDOUT = io.stdout
local peek = subprocess.peek
if platform.os == "windows" then
    local windows = require 'bee.windows'
    windows.filemode(STDIN, 'b')
    windows.filemode(STDOUT, 'b')
end
STDIN:setvbuf 'no'
STDOUT:setvbuf 'no'

local function send(v)
    STDOUT:write(v)
end
local function recv()
    local n = peek(STDIN)
    if n == nil or n == 0 then
        return ""
    end
    return STDIN:read(n)
end

local m = {}
local stat = {}
function m.debug(v)
    stat.debug = v
end
function m.sendmsg(pkg)
    send(proto.send(pkg, stat))
end
function m.recvmsg()
    return proto.recv(recv(), stat)
end

return m
