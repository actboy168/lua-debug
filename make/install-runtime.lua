local platform, arch = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

if platform == 'macos' then
    -- TODO
    return
end

local msvc = require 'msvc'
local output = CWD / 'publish' / 'runtime' / (arch == 'x86' and 'win32' or 'win64')
local bindir = CWD / 'build' / 'msvc' / 'bin' / 'runtime' / arch

fs.create_directories(output)
fs.copy_file(bindir / 'lua53.dll', output / 'lua53.dll', true)
fs.copy_file(bindir / 'lua54.dll', output / 'lua54.dll', true)
fs.copy_file(bindir / 'exe-lua53.exe', output / 'lua53.exe', true)
fs.copy_file(bindir / 'exe-lua54.exe', output / 'lua54.exe', true)
fs.copy_file(bindir / 'debugger.dll', output / 'debugger.dll', true)
fs.copy_file(bindir / 'debugger-inject.dll', output / 'debugger-inject.dll', true)

local function copy_crtdll(platform, target)
    fs.create_directories(target)
    fs.copy_file(msvc.crtpath(platform) / 'msvcp140.dll', target / 'msvcp140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'concrt140.dll', target / 'concrt140.dll', true)
    fs.copy_file(msvc.crtpath(platform) / 'vcruntime140.dll', target / 'vcruntime140.dll', true)
    for dll in msvc.ucrtpath(platform):list_directory() do
        fs.copy_file(dll, target / dll:filename(), true)
    end
end

copy_crtdll(arch, output)
