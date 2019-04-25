-- Just an example
local rdebug = require "remotedebug"
return (loadfile "script/start_debug.lua")(rdebug, ".", "/script/?.lua", "/?.dll")
