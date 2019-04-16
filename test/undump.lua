-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua
-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua

package.path = "extension/script/backend/?.lua"
local undump = require "undump"

local function getproto(f)
    local cl = undump(string.dump(f))
    return cl.f
end

getproto(function()end)
getproto(loadfile "extension/script/backend/undump.lua")

print "ok"
