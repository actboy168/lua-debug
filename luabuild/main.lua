print 'Step 1. init'

local fs = require 'bee.filesystem'

local configuration = arg[1] or 'Release'
local rebuild = arg[2] ~= 'IC'
local insiders = false
local vscode = insiders and '.vscode-insiders' or '.vscode'
local root = fs.absolute(fs.path '.')
local binDir = root / 'project' / 'windows' / 'bin'
local objDir = root / 'project' / 'windows' / 'obj'

local msvc = require 'msvc'
if not msvc:initialize(141, 'ansi') then
    error('Cannot found Visual Studio Toolset.')
end

local version = (function()
    for line in io.lines((root / 'project' / 'windows' / 'common.props'):string()) do
        local ver = line:match('<Version>(%d+%.%d+%.%d+)</Version>')
        if ver then
            print('version: ', ver)
            return ver
        end
    end
    error 'Cannot found version in common.props.'
end)()

local outputDir
if configuration == 'Release' then
    outputDir = root / 'publish'
else
    outputDir = fs.path(os.getenv('USERPROFILE')) / vscode / 'extensions' / ('actboy168.lua-debug-' .. version)
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
        fs.remove_all(binDir)
    end
    fs.remove_all(objDir / configuration)
    fs.remove_all(outputDir)
end

print 'Step 3. update version'
local function update_version(filename, pattern)
    local str = io_load(filename)
    local find_pattern = pattern:gsub('[%^%$%(%)%%%.%[%]%+%-%?]', '%%%0'):gsub('{}', '%%d+%%.%%d+%%.%%d+')
    local replace_pattern = pattern:gsub('{}', version)
    local first, last = str:find(find_pattern)
    if first then
        str = str:sub(1, first-1) .. replace_pattern .. str:sub(last+1)
        io_save(filename, str)
    else
        print(('Failed to write version into `%s`.'):format(filename:filename()))
    end
end
update_version(root / 'extension' / 'package.json', '"version": "{}"')
update_version(root / '.vscode' / 'launch.json', 'actboy168.lua-debug-{}')

print 'Step 4. copy extension'
copy_directory(root / 'extension', outputDir,
    function (path)
        local ext = path:extension():string():lower()
        return (ext ~= '.dll') and (ext ~= '.exe')
    end
)

print 'Step 5. copy crt dll'
if configuration == 'Release' then
    msvc:copy_crt_dll('x86', outputDir / 'runtime' / 'win32')
    msvc:copy_crt_dll('x64', outputDir / 'runtime' / 'win64')
    msvc:copy_crt_dll('x86', outputDir / 'bin' / 'win')
end

print 'Step 6. compile targetcpu = x86'
local property = {
    Configuration = configuration,
    Platform = 'Win32',
    VSCode = vscode
}
msvc:compile(rebuild and 'rebuild' or 'build', root / 'project' / 'vscode-lua-debug.sln', property)
if configuration == 'Release' then
    copy_directory(binDir / 'x86', outputDir / 'runtime' / 'win32',
        function (path)
            local ext = path:extension():string():lower()
            return (ext == '.dll') or (ext == '.exe')
        end
    )
end

print 'Step 7. compile targetcpu = x64'
local property = {
    Configuration = configuration,
    Platform = 'x64',
    VSCode = vscode
}
msvc:compile(rebuild and 'rebuild' or 'build', root / 'project' / 'vscode-lua-debug.sln', property)
if configuration == 'Release' then
    copy_directory(binDir / 'x64', outputDir / 'runtime' / 'win64',
        function (path)
            local ext = path:extension():string():lower()
            return (ext == '.dll') or (ext == '.exe')
        end
    )
end

print 'Step 8. compile bee'
local property = {
    Configuration = configuration,
    Platform = 'x86'
}
msvc:compile(rebuild and 'rebuild' or 'build', root / 'third_party' / 'bee.lua' / 'bee.sln', property)
local srcdir =  root / 'third_party' / 'bee.lua' / 'build' / ('msbuild_x86_' .. configuration) / 'bin'
local dstdir =  outputDir / 'bin' / 'win'
fs.create_directories(dstdir)
fs.copy_file(srcdir / 'lua.exe', dstdir / 'lua-debug.exe', true)
fs.copy_file(srcdir / 'bee.dll', dstdir / 'bee.dll', true)
fs.copy_file(srcdir / 'lua54.dll', dstdir / 'lua54.dll', true)
fs.copy_file(binDir / 'x86' / 'inject.dll', dstdir / 'inject.dll', true)

print 'finish.'
