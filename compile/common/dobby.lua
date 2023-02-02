local lm = require "luamake"
require 'compile.common.detect_platform'

local FullFloatingPointRegisterPack
local isArm64 = lm.runtime_platform == 'darwin-arm64' or lm.runtime_platform == 'linux-arm64'
if isArm64 then
    -- enable full foating point register
    FullFloatingPointRegisterPack = "-DFullFloatingPointRegisterPack=ON"
else
    FullFloatingPointRegisterPack = "-DFullFloatingPointRegisterPack=OFF"
end

return function(bindir)
    local option = {
        "cmake", "-S3rd/dobby", "-B" .. bindir,
        FullFloatingPointRegisterPack, "-DPlugin.SymbolResolver=ON", "-DPlugin.ImportTableReplace=ON",
        "-DCMAKE_BUILD_TYPE=Release",
        "-G",
        "Ninja"
    }
    if lm.runtime_platform:find("darwin") then
        table.insert(option, "-DBUILDING_SILICON=" .. "ON")
        table.insert(option, "-DCMAKE_OSX_ARCHITECTURES=" .. (isArm64 and "arm64" or "x86_64"))
    end

    table.insert(option, "&&cmake")
    table.insert(option, "--build")
    table.insert(option, bindir)
    table.insert(option, "-j4")
    lm:build "build_dobby" (option)
end
