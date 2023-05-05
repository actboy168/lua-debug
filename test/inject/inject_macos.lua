package.path = "extension/script/?.lua;extension/script/?/init.lua;" .. package.path
local fs = require "bee.filesystem"
local process_inject = require("frontend.process_inject")
local arch = require "bee.platform".Arch
if arch == "x86_64" then
    arch = "x64"
end
local dylib = tostring(fs.current_path() / "test/inject/launcher.so")

local function get_pid(nokill)
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
                    return setmetatable({ pid = l:sub(s, e) }, {
                        __gc = not nokill and function(t)
                            os.execute("kill -KILL " .. t.pid .. " 2> /dev/null")
                        end,
                        __tostring = function(t)
                            return tostring(t.pid)
                        end
                    })
                end
            end
        end
    end
end

local function compile_mask_so()
    print("mask so: " .. dylib)
    assert(os.execute("clang++ -shared -fPIC -g -O0 -std=c++17 test/inject/launcher.cpp -o " .. dylib))
    return setmetatable({}, {
        __gc = function()
            os.execute("rm -rf " .. dylib)
        end
    })
end

local function create_test_lua()
    local lua_path = "publish/runtime/darwin-" .. arch .. "/lua54/lua"
    local lua_handler = io.popen(lua_path .. ' -e "while true do end" &')
    lua_handler:close()
    return get_pid()
end

local function wait_injected(timeout)
    print("wait injected")
    for _ = 1, timeout do
        if fs.exists(dylib) then
            os.execute("sleep 1")
        else
            print("injected successfully")
            return true
        end
    end
end

local function test_inject(injecter, timeout)
    local _ = compile_mask_so()
    local pid_t = create_test_lua()
    local pid = pid_t.pid
    assert(pid)
    print("target pid:", pid)


    WORKDIR = fs.current_path() / "publish"
    assert(injecter(pid, "attach", dylib))
    print("process injecte: ", pid)
    assert(wait_injected(timeout or 5), "inject failed")
end

print ("----test lldb inject----")
test_inject(process_inject.lldb_inject, 10)
print ("----test macos inject----")
test_inject(process_inject.macos_inject)
