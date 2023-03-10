local fs = require "bee.filesystem"
local sp = require "bee.subprocess"

local all_tests = {}
local function find_directory_test(dir)
    for file in fs.pairs(dir) do
        if fs.is_directory(file) then
            find_directory_test(file)
        else
            local filename = file:filename()
            local ext = tostring(file:extension())
            if tostring(filename):find("test_") and (ext:find(".exe") or ext == "" or ext == nil )then
                table.insert(all_tests, tostring(file))
            end
        end
    end
end

find_directory_test("build")

local function run_tests()
    local res = true
    for i, test in ipairs(all_tests) do
        print(("%d/%d : %s"):format(i, #all_tests, test))
        local p, err = sp.spawn({
            test,
        })
        if not p then
            res = false
            print("can't run:", test, "err:", err)
        else
            local ec = p:wait()
            if ec ~= 0 then
                print("test return not 0")
                res = false
            end
        end
    end
    return res
end

if not run_tests() then
    os.exit(1)
end
