require 'filesystem'

local root = fs.absolute(fs.path '../')

local msvc = require 'msvc'
if not msvc:initialize(141, 'utf8') then
    error('Cannot found Visual Studio Toolset.')
end

local function copy_directory(from, to, filter)
    fs.create_directories(to)
    for fromfile in from:list_directory() do
        if fs.is_directory(fromfile) then
            copy_directory(fromfile, to / fromfile:filename(), filter)
        else
            if (not filter) or filter(fromfile) then
                fs.copy_file(fromfile, to / fromfile:filename(), true)
            end
        end
    end
end

copy_directory(root / 'extension', root / 'publish')
msvc:copy_crt_dll('x86', root / 'publish' / 'windows' / 'x86')
msvc:copy_crt_dll('x64', root / 'publish' / 'windows' / 'x64')
msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', 'Release', 'Win32')
msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', 'Release', 'x64')

print 'ok'