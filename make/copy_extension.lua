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

local DBG = root / 'extension' / "script" / "dbg.lua"
for _, platform in ipairs {"linux","macos","win32","win64"} do
    for _, luaver in ipairs {"lua51","lua52","lua53","lua54"} do
        local dir = outputDir / "runtime" / platform / luaver
        fs.create_directories(dir)
        fs.copy_file(DBG, dir / "dbg.lua", true)
    end
end
