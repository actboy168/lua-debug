package.path = package.path..";3rd/json.lua/?.lua"

local fs = require 'bee.filesystem'
local OS = require 'bee.platform'.os

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
    for fromfile in fs.pairs(from) do
        if (not filter) or filter(fromfile) then
            if fs.is_directory(fromfile) then
                copy_directory(fromfile, to / fromfile:filename(), filter)
            else
                local filename = fromfile:filename()
                if not crtdll(filename:string():lower()) then
                    print('copy', fromfile, to / filename)
                end
                if OS == "macos" then
                    -- must remove first, it will cause a code signature error
                    fs.remove(to / filename)
                end
                fs.copy_file(fromfile, to / filename, fs.copy_options.overwrite_existing)
            end
        end
    end
end

local sourceDir, extensionDir = ...
local sourceDir = fs.path(sourceDir)
copy_directory(sourceDir, extensionDir, function (path)
    local ext = path:extension()
    return ext ~= '.log' and path ~= sourceDir / "tmp"
end)

print 'ok'
