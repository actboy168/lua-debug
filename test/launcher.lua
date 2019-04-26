package.cpath = "build/linux/bin/runtime/x64/lua54/?.so"
package.path = "extension/script/?.lua"

local rdebug = require "remotedebug"
local dbg = (loadfile "extension/script/start_debug.lua")(rdebug, "test", "/../"..package.path, "/../"..package.cpath)
dbg:io("listen:127.0.0.1:4278")
dbg:wait()
dbg:start()

print "ok"
