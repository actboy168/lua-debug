local port = ...
local fs = require 'bee.filesystem'
local socket = require 'common.socket'
local proxy = require 'frontend.proxy'
local vscode
WORKDIR = fs.exe_path():parent_path():parent_path()

local function run()
    if port then
        vscode = socket('listen:127.0.0.1:'..port)
    else
        vscode = require 'frontend.stdio'
        --vscode.debug(true)
    end
    proxy.init(vscode)

    while true do
        proxy.update()
    end
end

local log = require 'common.log'
log.root = (WORKDIR / "script"):string()
log.file = (WORKDIR / "client.log"):string()

local ok, errmsg = xpcall(run, debug.traceback)
if not ok then
    log.error(errmsg)
end
