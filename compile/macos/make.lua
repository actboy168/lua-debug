local lm = require "luamake"

if lm.target == nil then
    local function getMacosVersion()
        local f = io.popen("sw_vers", "r")
        if f then
            for line in f:lines() do
                local major, minor, patch = line:match "ProductVersion:%s*(%d+)%.(%d+)%.(%d+)"
                if major and minor and patch then
                    return tonumber(major)*10000 + tonumber(minor)*100 + tonumber(patch)
                end
            end
        end
        return 0
    end
    if getMacosVersion() >= 110000 then
        require "compile.macos.macos11"
        return
    end
    require "compile.macos.macos10"
    return
end

local arch = lm.target:match "^([^-]*)-"
lm.builddir = ("build/macos/%s/%s"):format(lm.mode, arch)
require "compile.common.runtime"
