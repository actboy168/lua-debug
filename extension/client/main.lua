print = nil

local proxy = require 'proxy'
--local io = require 'io.stdio' ()
local io = require 'io.tcp_server' ('127.0.0.1', 26644)

proxy.initialize(io)

while true do
    proxy.update()
end
