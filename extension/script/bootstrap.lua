local root; do
    local sep = package.config:sub(1,1)
    local pattern = "["..sep.."][^"..sep.."]+"
    root = package.cpath:match("(.+)"..pattern..pattern..pattern.."$")
end
package.path = root .. "/script/?.lua"
dofile(root .. "/script/frontend/main.lua")
