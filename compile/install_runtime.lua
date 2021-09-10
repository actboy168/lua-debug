local bindir, arch = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local OS = require 'bee.platform'.OS:lower()

local exe = OS == 'windows' and ".exe" or ""
local dll = OS == 'windows' and ".dll" or ".so"
local OVERWRITE <const> = fs.copy_options.overwrite_existing

local ArchAlias = {
    x86_64 = "x64",
    x86 = "x86",
}

do
    --copy runtime
    for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
        local input = fs.path(bindir) / 'runtime' / luaver
        local output = CWD / 'publish' / 'runtime' / OS / arch / luaver
        fs.create_directories(output)
        fs.copy_file(input / ('lua'..exe),         output / ('lua'..exe),         OVERWRITE)
        fs.copy_file(input / ('remotedebug'..dll), output / ('remotedebug'..dll), OVERWRITE)
        if OS == 'windows' then
            fs.copy_file(input / (luaver..'.dll'), output / (luaver..'.dll'), OVERWRITE)
        end
    end
end

if OS == 'windows' then
    require 'msvc'.copy_vcrt(ArchAlias[arch], CWD / 'publish' / 'vcredist' / arch)
end
