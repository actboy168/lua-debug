local arch = require "bee.platform".Arch
if arch == "x86_64" then
    arch = "x64"
end
local fs = require "bee.filesystem"
local dylib = tostring(fs.current_path() / "test/inject/launcher.so")
print("mask so: " .. dylib)

local function get_pid()
    local ps = io.popen("ps awxx -o pid=,args= -c | grep 'while true'")
    if ps then
        while true do
            local l = ps:read "l"
            if not l then
                break
            end
            print(l)
            if l:find("%d+ lua %-e while true do end") then
                local s, e = l:find("%d+")
                if s and e then
                    return l:sub(s, e)
                end
            end
        end
    end
end

local function compile_mask_so()
    assert(os.execute("clang++ -shared -fPIC -g -O0 -std=c++17 test/inject/launcher.cpp -o " .. dylib))
    return setmetatable({}, {
            __gc = function()
                --os.execute("rm -rf " .. dylib)
            end
        })
end


local function create_test_lua()
    local lua_path = "publish/runtime/darwin-" .. arch .. "/lua54/lua"
    local lua_handler = io.popen(lua_path .. ' -e "while true do end" &')
    lua_handler:close()
    return get_pid()
end

local masker = compile_mask_so()
local pid = create_test_lua()
assert(pid)
print("target pid:" .. pid)

local function process_inject()
    local helper = "publish/bin/macos/process_inject_helper"
    local shell = ([[/usr/bin/osascript -e "do shell script \"%s %d %s %s\" with administrator privileges with prompt \"lua-debug\""]])
        :format(helper, pid, dylib, "attach")

    os.execute(shell)
end

local function wait_injected()
    print("wait injected")
    for i = 1, 5 do
        if fs.exists(dylib) then
            os.execute("sleep 1")
        else
            print("injected successfully")
            return true
        end
    end
end

process_inject()
print("process injecte: " .. pid)
assert(wait_injected())
