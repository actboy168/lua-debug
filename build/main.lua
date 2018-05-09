print 'Step 1. init'

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

print 'Step 2. remove old file'
fs.remove_all(root / 'project' / 'bin')
fs.remove_all(root / 'project' / 'obj' / 'Release')
fs.remove_all(root / 'publish')

print 'Step 3. compile targetcpu = x86'
msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', 'Release', 'Win32')
copy_directory(root / 'project' / 'bin' / 'x86', root / 'publish' / 'windows' / 'x86',
    function (path)
        local ext = path:extension():string():lower()
        return (ext == '.dll') or (ext == '.exe')
    end
)

print 'Step 4. compile targetcpu = x64'
msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', 'Release', 'x64')
copy_directory(root / 'project' / 'bin' / 'x64', root / 'publish' / 'windows' / 'x64',
    function (path)
        local ext = path:extension():string():lower()
        return (ext == '.dll') or (ext == '.exe')
    end
)

print 'Step 5. copy extension'
copy_directory(root / 'extension', root / 'publish')

print 'Step 6. copy crt dll'
msvc:copy_crt_dll('x86', root / 'publish' / 'windows' / 'x86')
msvc:copy_crt_dll('x64', root / 'publish' / 'windows' / 'x64')

print 'finish.'
