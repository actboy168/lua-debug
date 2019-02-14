local fs = require 'bee.filesystem'

local insiders = false
local vscode = insiders and '.vscode-insiders' or '.vscode'
local root = fs.absolute(fs.path '.')

local version = (function()
    for line in io.lines((root / 'project' / 'windows' / 'common.props'):string()) do
        local ver = line:match('<Version>(%d+%.%d+%.%d+)</Version>')
        if ver then
            return ver
        end
    end
    error 'Cannot found version in common.props.'
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

local outputDir = fs.path(os.getenv('USERPROFILE')) / vscode / 'extensions' / ('actboy168.lua-debug-' .. version)

copy_directory(root / 'extension', outputDir)

print 'ok'