local platform, luamake = ...

if not platform then
    local OS = require 'bee.platform'.OS
    if OS == 'Windows' then
        platform = 'msvc'
    elseif OS == 'macOS' then
        platform = 'macos'
    elseif OS == 'Linux' then
        platform = 'linux'
    end
end

if not luamake then
    luamake = 'luamake'
end

print 'Step 1. init'

local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local root = fs.absolute(fs.path '.')
local outputDir = root / 'publish'

local version = (function()
    for line in io.lines((root / 'extension' / 'package.json'):string()) do
        local ver = line:match('"version": "(%d+%.%d+%.%d+)"')
        if ver then
            return ver
        end
    end
    error 'Cannot found version in package.json.'
end)()

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

local function io_load(filepath)
    local f = assert(io.open(filepath:string(), 'rb'))
    local buf = f:read 'a'
    f:close()
    return buf
end

local function io_save(filepath, buf)
    local f = assert(io.open(filepath:string(), 'wb'))
    f:write(buf)
    f:close()
end

local function spawn(t)
    local process = assert(sp.spawn(t))
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end

print 'Step 2. remove old file'
fs.remove_all(outputDir)

print 'Step 3. update version'
local function update_version(filename, pattern)
    local str = io_load(filename)
    local find_pattern = pattern:gsub('[%^%$%(%)%%%.%[%]%+%-%?]', '%%%0'):gsub('{}', '%%d+%%.%%d+%%.%%d+')
    local replace_pattern = pattern:gsub('{}', version)
    local t = {}
    while true do
        local first, last = str:find(find_pattern)
        if first then
            t[#t+1] = str:sub(1, first-1)
            t[#t+1] = replace_pattern
            str = str:sub(last+1)
        else
            break
        end
    end
    t[#t+1] = str
    io_save(filename, table.concat(t))
end

update_version(root / '.vscode' / 'launch.json', 'actboy168.lua-debug-{}')

print 'Step 4. copy extension'
copy_directory(root / 'extension', outputDir,
    function (path)
        local ext = path:extension():string():lower()
        return (ext ~= '.dll') and (ext ~= '.exe')
    end
)
fs.copy_file(root / "LICENSE", outputDir / "LICENSE", true)

if platform == 'msvc' then
    print 'Step 5. compile launcher x86'
    spawn {
        luamake, 'remake', '-f', 'make-launcher.lua', '-arch', 'x86',
        cwd = root,
        searchPath = true,
    }
    print 'Step 6. compile launcher x64'
    spawn {
        luamake, 'remake', '-f', 'make-launcher.lua', '-arch', 'x64',
        cwd = root,
        searchPath = true,
    }
    print 'Step 7. compile bee'
else
    print 'Step 5. compile bee'
end

spawn {
    luamake, 'remake', '-f', 'make-bin.lua',
    cwd = root,
    searchPath = true,
}

if platform == 'msvc' then
    print 'Step 8. compile runtime x86'
    spawn {
        luamake, 'remake', '-f', 'make-runtime.lua', '-arch', 'x86',
        cwd = root,
        searchPath = true,
    }

    print 'Step 9. compile runtime x64'
    spawn {
        luamake, 'remake', '-f', 'make-runtime.lua', '-arch', 'x64',
        cwd = root,
        searchPath = true,
    }
else
    print 'Step 6. compile runtime'
    spawn {
        luamake, 'remake', '-f', 'make-runtime.lua',
        cwd = root,
        searchPath = true,
    }
end

print 'finish.'
