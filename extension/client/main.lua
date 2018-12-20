--print = nil

local select = require 'select'
local proxy = require 'proxy'
local serverFactory = require 'serverFactory'
local vscode = serverFactory {
    protocol = 'tcp',
    address = '127.0.0.1',
    port = 26644
}

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
