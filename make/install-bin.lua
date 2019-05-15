local platform = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

local bootstrap = CWD / "3rd" / "bee.lua" / "bootstrap"
fs.copy_file(bootstrap / "main.lua", CWD / 'build' / "main.lua", true)

if platform ~= 'msvc' then
    local output = CWD / 'publish' / 'bin' / platform
    local bindir = CWD / 'build' / platform / 'bin'

    fs.create_directories(output)
    fs.copy_file(bindir / 'bee.so', CWD / 'build' / 'bee.so', true)
    fs.copy_file(bindir / 'bootstrap', CWD / 'build' / 'lua', true)
    fs.copy_file(bindir / 'bee.so', output / 'bee.so', true)
    fs.copy_file(bindir / 'lua', output / 'lua-debug', true)
    fs.copy_file(bindir / 'inject.so', output / 'inject.so', true)
    return
end

local msvc = require 'msvc'
local output = CWD / 'publish' / 'bin' / 'win'
local bindir = CWD / 'build' / 'msvc' / 'bin'

fs.create_directories(output)
fs.copy_file(bindir / 'bee.dll', CWD / 'build' / 'bee.dll', true)
fs.copy_file(bindir / 'bootstrap.exe', CWD / 'build' / 'lua.exe', true)
fs.copy_file(bindir / 'bee.dll', output / 'bee.dll', true)
fs.copy_file(bindir / 'lua54.dll', output / 'lua54.dll', true)
fs.copy_file(bindir / 'lua.exe', output / 'lua-debug.exe', true)
fs.copy_file(bindir / 'inject.dll', output / 'inject.dll', true)

local function copy_crtdll(platform, target)
    fs.create_directories(target)
    fs.copy_file(msvc.crtpath(platform) / 'msvcp140.dll', target / 'msvcp140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'concrt140.dll', target / 'concrt140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'vcruntime140.dll', target / 'vcruntime140.dll', true)
    for dll in msvc.ucrtpath(platform):list_directory() do
        fs.copy_file(dll, target / dll:filename(), true)
    end
end

copy_crtdll('x86', output)
