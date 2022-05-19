local lm = require "luamake"
require "compile.common.detect_platform"

local arch = lm.target_arch
if arch == "ia32" then
	arch = "x86"
end

local luaver = "luajit"
local LUAJIT_ENABLE_LUA52COMPAT = "LUAJIT_ENABLE_LUA52COMPAT"
local LUAJIT_NUMMODE = "LUAJIT_NUMMODE=2"
local luajitDir = '3rd/lua/' .. luaver .. "/src"

local _M = {
	arch = arch,
	LUAJIT_ENABLE_LUA52COMPAT = LUAJIT_ENABLE_LUA52COMPAT,
	LUAJIT_NUMMODE = LUAJIT_NUMMODE,
	luajitDir = luajitDir
}

return _M
