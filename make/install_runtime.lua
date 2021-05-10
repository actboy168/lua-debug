local builddir, target, mode = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local OS = require 'bee.platform'.OS:lower()
local ARCH
do
    if OS == 'windows' then
        if target == 'x86' then
            ARCH = 'x86'
        elseif target == 'x64' then
            ARCH = 'x86_64'
        end
    elseif OS == 'linux' then
        ARCH = 'x86_64'
    elseif OS == 'macos' then
        ARCH = target:match '^([^-]*)-'
    end
end

local exe = OS == 'windows' and ".exe" or ""
local dll = OS == 'windows' and ".dll" or ".so"

do
    --copy lua-debug
    local input = CWD / builddir / 'bin' / target / mode
    local output = CWD / 'publish' / 'bin' / OS
    fs.create_directories(output)
    if (OS == 'windows' and ARCH == 'x86') or (OS ~= 'windows' and ARCH == 'x86_64') then
        fs.create_directories(output)
        fs.copy_file(input / ('bee'..dll),       output / ('bee'..dll),        true)
        fs.copy_file(input / ('lua'..exe),       output / ('lua-debug'..exe),  true)
        if OS == 'windows' then
            fs.copy_file(input / 'inject.dll',  output / 'inject.dll', true)
            fs.copy_file(input / 'lua54.dll',   output / 'lua54.dll',  true)
        end
    end
    if OS == 'windows' then
        fs.copy_file(input / 'launcher.dll', output / ('launcher.'..target..'.dll'), true)
    end
end

do
    --copy runtime
    for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
        local input = CWD / builddir / 'bin' / target / mode / 'runtime' / luaver
        local output = CWD / 'publish' / 'runtime' / OS / ARCH / luaver
        fs.create_directories(output)
        fs.copy_file(input / ('lua'..exe),         output / ('lua'..exe),         true)
        fs.copy_file(input / ('remotedebug'..dll), output / ('remotedebug'..dll), true)
        if OS == 'windows' then
            fs.copy_file(input / (luaver..'.dll'), output / (luaver..'.dll'), true)
        end
    end
end

if OS == 'windows' then
    require 'msvc'.copy_vcrt(target, CWD / 'publish' / 'vcredist' / ARCH)
end
