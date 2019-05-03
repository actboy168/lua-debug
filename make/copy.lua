local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

local home = fs.path(platform.OS == "Windows" and os.getenv 'USERPROFILE' or os.getenv 'HOME')
local vscode = arg[2] and ('.vscode-'..arg[2]) or '.vscode'
--local vscode = '.vscode'
--local vscode = '.vscode-insiders'
--local vscode = '.vscode-remote'

local version = (function()
    for line in io.lines(arg[1] .. '/package.json') do
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
                print('copy', fromfile, to / fromfile:filename())
                fs.copy_file(fromfile, to / fromfile:filename(), true)
            end
        end
    end
end

local outputDir = home / vscode / 'extensions' / ('actboy168.lua-debug-' .. version)

copy_directory(fs.path(arg[1]), outputDir)

print 'ok'
