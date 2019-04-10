local platform, arch, luaver = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

if platform == 'macos' then
    -- TODO
    return
end

local msvc = require 'msvc'
local output = CWD / 'publish' / 'runtime' / (arch == 'x86' and 'win32' or 'win64') / luaver
local bindir = CWD / 'build' / 'msvc' / 'bin' / 'runtime' / arch / luaver

fs.create_directories(output)
fs.copy_file(bindir / (luaver..'.dll'), output / (luaver..'.dll'), true)
fs.copy_file(bindir / 'lua.exe', output / 'lua.exe', true)
if luaver == "lua53" then
    fs.copy_file(bindir / 'debugger.dll', output / 'debugger.dll', true)
    fs.copy_file(bindir / 'debugger-inject.dll', output / 'debugger-inject.dll', true)
end

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
