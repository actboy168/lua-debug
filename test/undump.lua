-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua
-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua

package.path = "extension/script/?.lua"
local undump = require "backend.undump"
local parser = require "backend.parser"

local function getproto(f)
    local cl = undump(string.dump(f))
    return cl.f
end

getproto(function()end)
getproto(loadfile "extension/script/backend/undump.lua")

local src = {}
parser(src, loadfile "extension/script/frontend/mainloop.lua")
assert(src.activelines[30] == false)
assert(src.activelines[32] == true)

print "ok"
