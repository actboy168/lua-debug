local dbg = require "debugger"
dbg:start "127.0.0.1:12306"

dbg:event "wait" --@A

local function f()
    print(111) --@B
end
local co = coroutine.create(function ()
    f()
end)

dbg:event "wait" --@C

coroutine.resume(co)
