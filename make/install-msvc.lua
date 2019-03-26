local fs = require 'bee.filesystem'
local msvc = require 'msvc'

local CWD = fs.current_path()
local output = CWD / 'publish' / 'bin' / 'win'
local bindir = CWD / 'build' / 'msvc' / 'bin'

fs.create_directories(output)
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
