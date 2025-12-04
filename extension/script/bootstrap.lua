local root; do
    local pattern = "[/][^/]+"
    root = package.cpath:match("(.+)"..pattern..pattern.."$")
end
package.path = root.."/script/?.lua"

for i = 1, #arg do
    if arg[i] == '-e' then
        local expr = assert(arg[i + 1], "'-e' needs argument")
        assert(load(expr, "=(command line)"))()
        table.remove(arg, i + 1)
        table.remove(arg, i)
        break
    end
end

for i = #arg, 1, -1 do
    if arg[i] == '' then
        table.remove(arg, i)
    end
end

local func = assert(loadfile(root.."/script/frontend/main.lua"))
func(arg[1])
