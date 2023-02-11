local fs = require "bee.filesystem"
local arch = require "bee.platform".Arch
local path = 'test/load/test'


local x86_64 = {}
local arm64 = {}
local function find_directory_so(dir)
    for file in fs.pairs(dir) do
        if fs.is_directory(file) then
            find_directory_so(file)
        else
            local extension = file:extension()
            if tostring(extension) == ".so" then
                local filepath = tostring(file)
                local t = filepath:find("x64") and x86_64 or arm64
                if filepath:find("remote") then
                    table.insert(t, { tostring(file:replace_filename("lua")), filepath })
                else
                    table.insert(t, filepath)
                end
            end
        end
    end
end

find_directory_so("publish")
local count = #x86_64 + #arm64
local index = 1;
local function load_test(paths, arch)
    local target = ("%s_%s"):format(path, arch)
    assert(os.execute(("clang %s.cpp -o %s -arch %s"):format(path, target, arch)))
    for _, file in ipairs(paths) do
        if type(file) == "table" then
            file = table.concat(file, " ")
        end
        local shell = ("%s %s"):format(target, file)
        print(index .. "/" .. count .. " " .. shell)
        assert(os.execute(shell))
        index = index + 1
    end
    os.execute(("rm -rf %s"):format(target))
end


load_test(x86_64, "x86_64")
if arch == "arm64" then
    load_test(arm64, "arm64")
end
