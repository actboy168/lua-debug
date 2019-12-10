local fs = require 'bee.filesystem'
local root = fs.absolute(fs.path '.')
local outputDir = root / 'publish'

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

copy_directory(root / 'extension', outputDir,
function (path)
    local ext = path:extension():string():lower()
    return (ext ~= '.dll') and (ext ~= '.exe')
end
)
fs.copy_file(root / "LICENSE", outputDir / "LICENSE", true)
