local platform = require 'bee.platform'

local m = {
    OS = platform.OS,
    useWSL = false,
}

function m.init(args)
    if platform.OS == "Windows" and args.useWSL then
        m.OS = "Linux"
        m.useWSL = true
        return
    end
    m.OS = platform.OS
    m.useWSL = false
end

return setmetatable(m, {__call = function() return m.OS end})
