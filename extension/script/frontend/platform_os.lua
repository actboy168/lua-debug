local platform = require 'bee.platform'

local m = {
    os = platform.os,
}

function m.init(args)
    if platform.os == "windows" and args.useWSL then
        m.os = "linux"
        args.useWSL = true
        return
    end
    m.os = platform.os
    args.useWSL = nil
end

return setmetatable(m, {__call = function() return m.os end})
