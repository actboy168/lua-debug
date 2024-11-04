local lm = require "luamake"
local fs = require 'bee.filesystem'

local arch = lm.runtime_platform:sub(lm.runtime_platform:find("-") + 1)
if arch == "ia32" then
	arch = "x86"
end

local luaver = "luajit"
local LUAJIT_ENABLE_LUA52COMPAT = "LUAJIT_ENABLE_LUA52COMPAT"
local LUAJIT_NUMMODE = "LUAJIT_NUMMODE=2"
local luajitDir = '3rd/lua/' .. luaver .. "/src"
local is_old_version_luajit = fs.exists(fs.path(luajitDir) / 'lj_init.c')

local _M = {
	arch = arch,
	LUAJIT_ENABLE_LUA52COMPAT = LUAJIT_ENABLE_LUA52COMPAT,
	LUAJIT_NUMMODE = LUAJIT_NUMMODE,
	luajitDir = luajitDir,
	is_old_version_luajit = is_old_version_luajit
}

return _M
