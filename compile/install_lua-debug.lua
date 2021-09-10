local bindir = ...
local fs = require 'bee.filesystem'
local CWD = fs.current_path()
local OS = require 'bee.platform'.OS:lower()
local exe = OS == 'windows' and ".exe" or ""
local OVERWRITE <const> = fs.copy_options.overwrite_existing

local input = fs.path(bindir)
local output = CWD / 'publish' / 'bin' / OS
fs.create_directories(output)
fs.copy_file(input / ('lua-debug'..exe), output / ('lua-debug'..exe), OVERWRITE)
fs.copy_file(CWD / "extension" / "script" / "bootstrap.lua", output / 'main.lua', OVERWRITE)
if OS == 'windows' then
    fs.copy_file(input / 'inject.dll',  output / 'inject.dll', OVERWRITE)
    fs.copy_file(input / 'lua54.dll',   output / 'lua54.dll',  OVERWRITE)
end
