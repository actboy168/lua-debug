local platform = require 'bee.platform'

local OS = platform.OS

local function init(args)
    if platform.OS == "Windows" and args.useWSL then
        OS = "Linux"
        return
    end
    OS = platform.OS
end

return setmetatable({
    init = init,
}, {__call = function() return OS end})
