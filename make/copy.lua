local fs = require 'bee.filesystem'

local function getExtensionDirName(packageDir)
    local publisher,name,version
    for line in io.lines(packageDir .. '/package.json') do
        if not publisher then
            publisher = line:match('"publisher": "([^"]+)"')
        end
        if not name then
            name = line:match('"name": "([^"]+)"')
        end
        if not version then
            version = line:match('"version": "(%d+%.%d+%.%d+)"')
        end
    end
    if not publisher then
        error 'Cannot found `publisher` in package.json.'
    end
    if not name then
        error 'Cannot found `name` in package.json.'
    end
    if not version then
        error 'Cannot found `version` in package.json.'
    end
    return ('%s.%s-%s'):format(publisher,name,version)
end

local function crtdll(path)
    if path:find("api-ms-win-", 1, true) then
        return true
    end
    if path:sub(1, 5) == "msvcp" then
        return true
    end
    if path:sub(1, 9) == "vcruntime" then
        return true
    end
    if path == "concrt140.dll" then
        return true
    end
    if path == "ucrtbase.dll" then
        return true
    end
    return false
end

local function copy_directory(from, to, filter)
    fs.create_directories(to)
    for fromfile in from:list_directory() do
        if fs.is_directory(fromfile) then
            copy_directory(fromfile, to / fromfile:filename(), filter)
        else
            if (not filter) or filter(fromfile) then
                local filename = fromfile:filename()
                if not crtdll(filename:string():lower()) then
                    print('copy', fromfile, to / filename)
                end
                fs.copy_file(fromfile, to / filename, true)
            end
        end
    end
end

local packageDir,sourceDir,extensionPath = ...
local extensionDirName = getExtensionDirName(packageDir)
local extensionDir = fs.path(extensionPath) / extensionDirName
sourceDir = fs.path(sourceDir)
if not fs.exists(extensionDir) then
    error("`" .. extensionDir:string() .. "` is not installed.")
end

copy_directory(sourceDir, extensionDir)

print 'ok'
