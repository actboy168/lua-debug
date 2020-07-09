local platform = require "bee.platform".OS:lower()
local COMMAND = {
    windows = "wmic process get name,processid",
    linux = "ps axww -o comm=,pid=",
    macos = "ps axww -o comm=,pid= -c",
}
return function (n)
    local res = {}
    local f = assert(io.popen(COMMAND[platform]))
    if platform == "windows" then f:read "l" end
    for line in f:lines() do
        local name, processid = line:match "^([^%s].*[^%s])%s+(%d+)%s*$"
        if n == name then
            res[#res+1] = tonumber(processid)
        end
    end
    return res
end
