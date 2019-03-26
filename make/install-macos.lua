local fs = require 'bee.filesystem'

local CWD = fs.current_path()
local output = CWD / 'publish' / 'bin' / 'macos'
local bindir = CWD / 'build' / 'macos' / 'bin'

fs.create_directories(output)
fs.copy_file(bindir / 'bee.so', output / 'bee.so', true)
fs.copy_file(bindir / 'lua', output / 'lua-debug', true)
