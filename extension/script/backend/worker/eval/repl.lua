local source, level = ...
level = (level or 0) + 2

if _VERSION == "Lua 5.1" then
	load = loadstring
	function table.pack(...)
		local t = {...}
		t.n = select("#", ...)
		return t
	end
	table.unpack = unpack
end

local f = assert(debug.getinfo(level,"f").func, "can't find function")
local args_name = {}
local args_value = {}
local locals = {}
do
	local i = 1
	while true do
		local name, value = debug.getupvalue(f, i)
		if name == nil then
			break
		end
		if #name > 0 then
			args_name[#args_name+1] = name
			args_value[name] = value
		end
		i = i + 1
	end
end
do
	local i = 1
	while true do
		local name, value = debug.getlocal(level, i)
		if name == nil then
			break
		end
		if name:byte() ~= 40 then	-- '('
			args_name[#args_name+1] = name
			args_value[name] = value
			locals[#locals+1] = ("[%d]={%s},"):format(i,name)
		end
		i = i + 1
	end
end

local full_source
if #args_name > 0 then
	full_source = ([[
local $ARGS
return function(...)
$SOURCE
end,
function()
return {$LOCALS}
end
]]):gsub("%$(%w+)", {
	ARGS = table.concat(args_name, ","),
	SOURCE = source,
	LOCALS = table.concat(locals),
})
else
	full_source = ([[
return function(...)
$SOURCE
end,
function()
return {$LOCALS}
end
]]):gsub("%$(%w+)", {
	SOURCE = source,
	LOCALS = table.concat(locals),
})
end

local func, update = assert(load(full_source, '=(EVAL)'))()
do
	local i = 1
	while true do
		local name = debug.getupvalue(func, i)
		if name == nil then
			break
		end
		debug.setupvalue(func, i, args_value[name])
		i = i + 1
	end
end
local rets
do
	local vararg, v = debug.getlocal(level, -1)
	if vararg then
		local vargs = { v }
		local i = 2
		while true do
			vararg, v = debug.getlocal(level, -i)
			if vararg then
				vargs[i] = v
			else
				break
			end
			i = i + 1
		end
		rets = table.pack(func(table.unpack(vargs)))
	else
		rets = table.pack(func())
	end
end
for k,v in pairs(update()) do
	debug.setlocal(level,k,v[1])
end
return table.unpack(rets)
