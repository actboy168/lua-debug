print 'Step 1. init'

require 'filesystem'

local configuration = arg[1] or 'Release'
local rebuild = arg[2] ~= 'IC'

local root = fs.absolute(fs.path '../')

local msvc = require 'msvc'
if not msvc:initialize(141, 'utf8') then
    error('Cannot found Visual Studio Toolset.')
end

local version = (function()
    for line in io.lines((root / 'project' / 'common.props'):string()) do
        local ver = line:match('<Version>(%d+%.%d+%.%d+)</Version>')
        if ver then
            return ver
        end
    end
    error 'Cannot found version in common.props.'
end)()

local outputDir
if configuration == 'Release' then
    outputDir = root / 'publish'
else
    outputDir = fs.path(os.getenv('USERPROFILE')) / '.vscode' / 'extensions' / ('actboy168.lua-debug-' .. version)
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


function io_load(filepath)
    local f = assert(io.open(filepath:string(), 'rb'))
    local buf = f:read 'a'
    f:close()
    return buf
end

function io_save(filepath, buf)
    local f = assert(io.open(filepath:string(), 'wb'))
    f:write(buf)
    f:close()
end

print 'Step 2. remove old file'
if rebuild then
    if configuration == 'Release' then
        fs.remove_all(root / 'project' / 'bin')
    end
    fs.remove_all(root / 'project' / 'obj' / configuration)
    fs.remove_all(outputDir)
end

print 'Step 3. compile targetcpu = x86'
if rebuild then
    msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', configuration, 'Win32')
else
    msvc:build(root / 'project' / 'vscode-lua-debug.sln', configuration, 'Win32')
end
if configuration == 'Release' then
    copy_directory(root / 'project' / 'bin' / 'x86', outputDir / 'windows' / 'x86',
        function (path)
            local ext = path:extension():string():lower()
            return (ext == '.dll') or (ext == '.exe')
        end
    )
end

print 'Step 4. compile targetcpu = x64'
if rebuild then
    msvc:rebuild(root / 'project' / 'vscode-lua-debug.sln', configuration, 'x64')
else
    msvc:build(root / 'project' / 'vscode-lua-debug.sln', configuration, 'x64')
end
if configuration == 'Release' then
    copy_directory(root / 'project' / 'bin' / 'x64', outputDir / 'windows' / 'x64',
        function (path)
            local ext = path:extension():string():lower()
            return (ext == '.dll') or (ext == '.exe')
        end
    )
end

print 'Step 5. copy extension'
local str = io_load(root / 'extension' / 'package.json')
local first, last = str:find '"version": "%d+%.%d+%.%d+"'
if first then
    str = str:sub(1, first-1) .. ('"version": "%s"'):format(version) .. str:sub(last+1)
    io_save(root / 'extension' / 'package.json', str)
else
    print 'Failed to write version into package.json.'
end

copy_directory(root / 'extension', outputDir)

print 'Step 6. copy crt dll'
if configuration == 'Release' then
    msvc:copy_crt_dll('x86', outputDir / 'windows' / 'x86')
    msvc:copy_crt_dll('x64', outputDir / 'windows' / 'x64')
end

print 'finish.'
