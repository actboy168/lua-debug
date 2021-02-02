-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua
-- publish\runtime\win32\lua53\lua.exe test\undump.lua
-- publish\runtime\win64\lua54\lua.exe test\undump.lua

package.path = "extension/script/?.lua"
local undump = require "backend.worker.undump"
local parser = require "backend.worker.parser"

package.loaded["backend.worker.evaluate"] = {
    dump = function(content)
        return true, string.dump(content)
    end,
}

local function getproto(f)
    local cl = undump(string.dump(f))
    return cl.f
end

getproto(function()end)
getproto(assert(loadfile "extension/script/backend/worker/undump.lua"))

local si = parser(assert(loadfile "extension/script/frontend/main.lua"))
assert(si["20-34"])
assert(si["20-34"][25] == nil)
assert(si["20-34"][27] == true)

print "ok"
