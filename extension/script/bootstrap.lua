local root; do
    local sep = package.config:sub(1,1)
    local pattern = "["..sep.."][^"..sep.."]+"
    root = package.cpath:match("(.+)"..pattern..pattern..pattern.."$")
end
package.path = root .. "/script/?.lua"

for i = 1, #arg do
    if arg[i] == '-e' then
        i = i + 1
        local expr = assert(arg[i], "'-e' needs argument")
        assert(load(expr, "=(command line)"))()
        break
    end
end

dofile(root .. "/script/frontend/main.lua")
