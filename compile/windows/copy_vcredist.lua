local fs = require 'bee.filesystem'
local CWD = fs.current_path()
local msvc = require 'msvc'
msvc.copy_vcrt("x64", CWD / 'publish' / 'vcredist' / "win32-x64")
msvc.copy_vcrt("x86", CWD / 'publish' / 'vcredist' / "win32-ia32")
