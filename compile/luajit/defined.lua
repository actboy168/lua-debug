local lm = require "luamake"

local arch = "x64"
if lm.platform == "linux-arm64" or lm.platform == "darwin-arm64" then
    arch = "arm64"
else
    assert(lm.platform == "darwin-x64" or lm.platform == "linux-x64", "unknown runtime platform: " .. lm.platform)
end

if lm.cross_arch then
    arch = lm.cross_arch
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
