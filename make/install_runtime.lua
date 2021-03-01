local platform, arch, mode = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local exe = platform == 'msvc' and ".exe" or ""
local dll = platform == 'msvc' and ".dll" or ".so"

if platform ~= 'msvc' or arch == 'x86' then
    local binplat = platform ~= 'msvc' and platform or 'win'
    local output = CWD / 'publish' / 'bin' / binplat
    local bindir = CWD / 'build' / platform / 'bin' / arch / mode
    fs.create_directories(output)
    fs.copy_file(bindir / ('bee'..dll),       output / ('bee'..dll),        true)
    fs.copy_file(bindir / ('lua'..exe),       output / ('lua-debug'..exe),  true)
    fs.copy_file(bindir / ('inject'..dll),    output / ('inject'..dll),     true)
    if platform == 'msvc' then
        fs.copy_file(bindir / 'lua54.dll',    output / 'lua54.dll',         true)
        require 'msvc'.copy_crtdll(arch, output)
    end
end

if platform == 'msvc' then
    local output = CWD / 'publish' / 'bin' / 'win'
    local bindir = CWD / 'build' / platform / 'bin' / arch / mode
    fs.create_directories(output)
    fs.copy_file(bindir / 'launcher.dll', output / ('launcher.'..arch..'.dll'), true)
end

for _, luaver in ipairs {"lua51","lua52","lua53","lua54","lua-latest"} do
    local rtplat = platform ~= 'msvc' and platform or (arch == 'x86' and 'win32' or 'win64')
    local bindir = CWD / 'build' / platform / 'bin' / arch / mode / 'runtime' / luaver
    local output = CWD / 'publish' / 'runtime' / rtplat / luaver
    fs.create_directories(output)
    fs.copy_file(bindir / ('lua'..exe),         output / ('lua'..exe),         true)
    fs.copy_file(bindir / ('remotedebug'..dll), output / ('remotedebug'..dll), true)
    if platform == 'msvc' then
        fs.copy_file(bindir / (luaver..'.dll'), output / (luaver..'.dll'), true)
        require 'msvc'.copy_crtdll(arch, output)
    end
end
