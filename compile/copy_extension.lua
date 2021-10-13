local fs = require 'bee.filesystem'
local root = fs.absolute(fs.path '.')
local outputDir = root / 'publish'
local OVERWRITE <const> = fs.copy_options.overwrite_existing

local function copy_directory(from, to)
    fs.create_directories(to)
    for fromfile in fs.pairs(from) do
        if fs.is_directory(fromfile) then
            copy_directory(fromfile, to / fromfile:filename())
        else
            fs.copy_file(fromfile, to / fromfile:filename(), OVERWRITE)
        end
    end
end

copy_directory(root / 'extension', outputDir)

fs.copy_file(root / "LICENSE",   outputDir / "LICENSE",   OVERWRITE)
fs.copy_file(root / "README.md", outputDir / "README.md", OVERWRITE)
