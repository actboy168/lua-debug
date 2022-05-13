local input, output, platform = ...

package.path = package.path ..";3rd/json.lua/?.lua"
local json = require "json-beautify"

local jsondata = assert(loadfile(input))(platform)
local f <close> = assert(io.open(output, "wb"))
f:write(json.beautify(jsondata))
