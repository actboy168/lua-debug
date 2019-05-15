local platform, arch = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()

for _, luaver in ipairs {"lua53","lua54"} do
    if platform ~= 'msvc' then
        local output = CWD / 'publish' / 'runtime' / platform / luaver
        local bindir = CWD / 'build' / platform / 'bin' / 'runtime' / arch / luaver

        fs.create_directories(output)
        fs.copy_file(bindir / 'lua', output / 'lua', true)
        fs.copy_file(bindir / 'remotedebug.so', output / 'remotedebug.so', true)
    else
        local msvc = require 'msvc'
        local output = CWD / 'publish' / 'runtime' / (arch == 'x86' and 'win32' or 'win64') / luaver
        local bindir = CWD / 'build' / 'msvc' / 'bin' / 'runtime' / arch / luaver

        fs.create_directories(output)
        fs.copy_file(bindir / (luaver..'.dll'), output / (luaver..'.dll'), true)
        fs.copy_file(bindir / 'lua.exe', output / 'lua.exe', true)
        fs.copy_file(bindir / 'remotedebug.dll', output / 'remotedebug.dll', true)

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
    end
end
