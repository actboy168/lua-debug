local fs = require 'bee.filesystem'
local root = fs.absolute(fs.path '.')

local version = dofile "compile/common/package_json.lua".version

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
