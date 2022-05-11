local input, output = ...

package.path = package.path ..";3rd/json.lua/?.lua"
local json = require "json-beautify"

local f <close> = assert(io.open(output, "wb"))
f:write(json.beautify(dofile(input)))
