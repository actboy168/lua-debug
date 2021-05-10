local bindir, arch = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local OS = require 'bee.platform'.OS:lower()

local exe = OS == 'windows' and ".exe" or ""
local dll = OS == 'windows' and ".dll" or ".so"

local ArchAlias = {
    x86_64 = "x64",
    x86 = "x86",
}

do
    --copy lua-debug
    local input = fs.path(bindir)
    local output = CWD / 'publish' / 'bin' / OS
    fs.create_directories(output)
    if (OS == 'windows' and arch == 'x86') or (OS ~= 'windows' and arch == 'x86_64') then
        fs.create_directories(output)
        fs.copy_file(input / ('bee'..dll),       output / ('bee'..dll),        true)
        fs.copy_file(input / ('lua'..exe),       output / ('lua-debug'..exe),  true)
        if OS == 'windows' then
            fs.copy_file(input / 'inject.dll',  output / 'inject.dll', true)
            fs.copy_file(input / 'lua54.dll',   output / 'lua54.dll',  true)
        end
    end
    if OS == 'windows' then
        fs.copy_file(input / 'launcher.dll', output / ('launcher.'..ArchAlias[arch]..'.dll'), true)
    end
end

do
    --copy runtime
    for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
        local input = fs.path(bindir) / 'runtime' / luaver
        local output = CWD / 'publish' / 'runtime' / OS / arch / luaver
        fs.create_directories(output)
        fs.copy_file(input / ('lua'..exe),         output / ('lua'..exe),         true)
        fs.copy_file(input / ('remotedebug'..dll), output / ('remotedebug'..dll), true)
        if OS == 'windows' then
            fs.copy_file(input / (luaver..'.dll'), output / (luaver..'.dll'), true)
        end
    end
end

if OS == 'windows' then
    require 'msvc'.copy_vcrt(ArchAlias[arch], CWD / 'publish' / 'vcredist' / arch)
end
