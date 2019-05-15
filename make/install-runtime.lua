local platform, arch = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local function copy_crtdll(platform, target)
    local msvc = require 'msvc'
    fs.create_directories(target)
    fs.copy_file(msvc.crtpath(platform) / 'msvcp140.dll', target / 'msvcp140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'concrt140.dll', target / 'concrt140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'vcruntime140.dll', target / 'vcruntime140.dll', true)
    for dll in msvc.ucrtpath(platform):list_directory() do
        fs.copy_file(dll, target / dll:filename(), true)
    end
end

local exe = platform == 'msvc' and ".exe" or ""
local dll = platform == 'msvc' and ".dll" or ".so"

if platform ~= 'msvc' or arch == 'x86' then
    local binplat = platform ~= 'msvc' and platform or 'win'
    local output = CWD / 'publish' / 'bin' / binplat
    local bindir = CWD / 'build' / platform / 'bin' / arch
    local bootstrap = CWD / "3rd" / "bee.lua" / "bootstrap"
    fs.create_directories(output)
    fs.copy_file(bootstrap / "main.lua",      CWD / 'build' / "main.lua",   true)
    fs.copy_file(bindir / ('bee'..dll),       CWD / 'build' / ('bee'..dll), true)
    fs.copy_file(bindir / ('bootstrap'..exe), CWD / 'build' / ('lua'..exe), true)
    fs.copy_file(bindir / ('bee'..dll),       output / ('bee'..dll),        true)
    fs.copy_file(bindir / ('lua'..exe),       output / ('lua-debug'..exe),  true)
    fs.copy_file(bindir / ('inject'..dll),    output / ('inject'..dll),     true)
    if platform == 'msvc' then
        fs.copy_file(bindir / 'lua54.dll', output / 'lua54.dll', true)
        copy_crtdll('x86', output)
    end
end

if platform == 'msvc' then
    local output = CWD / 'publish' / 'bin' / 'win'
    local bindir = CWD / 'build' / platform / 'bin' / arch
    fs.copy_file(bindir / 'launcher.dll', output / ('launcher.'..arch..'.dll'), true)
end

for _, luaver in ipairs {"lua53","lua54"} do
    local rtplat = platform ~= 'msvc' and platform or (arch == 'x86' and 'win32' or 'win64')
    local bindir = CWD / 'build' / platform / 'bin' / arch / 'runtime' / luaver
    local output = CWD / 'publish' / 'runtime' / rtplat / luaver
    fs.create_directories(output)
    fs.copy_file(bindir / ('lua'..exe),         output / ('lua'..exe),         true)
    fs.copy_file(bindir / ('remotedebug'..dll), output / ('remotedebug'..dll), true)
    if platform == 'msvc' then
        fs.copy_file(bindir / (luaver..'.dll'), output / (luaver..'.dll'), true)
        copy_crtdll(arch, output)
    end
end
