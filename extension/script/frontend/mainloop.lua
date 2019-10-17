local network = require 'common.network'
local proxy = require 'frontend.proxy'
local vscode

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

return function(port)
    if port then
        vscode = network.open('127.0.0.1:'..port)
    else
        vscode = require 'frontend.stdio'
        vscode.debug(true)
    end
    proxy.init(vscode)

    while true do
        network.update(0.05)
        proxy.update()
        update()
    end
end
