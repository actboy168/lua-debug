local source = ...

local _load
if _VERSION == "Lua 5.1" then
	_load = loadstring
else
	_load = load
end

assert(_load("return " .. source, '=(EVAL)'))
