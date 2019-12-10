local fs = require 'bee.filesystem'
local root = fs.absolute(fs.path '.')

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
