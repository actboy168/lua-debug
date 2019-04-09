local serverFactory = require 'frontend.serverFactory'
local select = require 'frontend.select'
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
        vscode = serverFactory {
            protocol = 'tcp',
            address = '127.0.0.1',
            port = tostring(port)
        }
    else
        vscode = require 'frontend.stdio'
        --vscode.debug(true)
    end
    proxy.init(vscode)

    while true do
        select.update(0.05)
        proxy.update()
        update()
    end
end
