local thread = require "bee.thread"
local thd = thread.thread [[dofile "thread.lua"]]
thread.wait(thd)
