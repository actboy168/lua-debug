local port = ...

local function run()
    (require 'mainloop')(port)
end

if port then
    run()
    return
end

local fs = require 'bee.filesystem'
local log = require 'log'
log.file = (fs.exe_path():parent_path():parent_path():parent_path() / "client.log"):string()

print = log.info
local ok, errmsg = xpcall(run, debug.traceback)
if not ok then
    log.error(errmsg)
end
