local port = ...

local fs = require 'bee.filesystem'
WORKDIR = fs.exe_path():parent_path():parent_path():parent_path()

local function run()
    (require 'mainloop')(port)
end

if port then
    run()
    return
end

local log = require 'log'
log.file = (WORKDIR / "client.log"):string()

print = log.info
local ok, errmsg = xpcall(run, debug.traceback)
if not ok then
    log.error(errmsg)
end
