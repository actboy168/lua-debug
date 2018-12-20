print = nil

local select = require 'select'
local proxy = require 'proxy'
local factory = require 'serverFactory'
local vscode = factory.tcp_server('127.0.0.1', 26644)

local function update()
    while true do
        local pkg = vscode.recv()
        if pkg then
            proxy.send(pkg)
        else
            return
        end
    end
end

proxy.init(vscode)

while true do
    select.update(0.05)
    proxy.update()
    update()
end
