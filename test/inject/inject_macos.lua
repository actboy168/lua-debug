package.path = "extension/script/?.lua;extension/script/?/init.lua;" .. package.path

local arch = require "bee.platform".Arch
if arch == "x86_64" then
    arch = "x64"
end
local fs = require "bee.filesystem"
local dylib = tostring(fs.current_path() / "test/inject/launcher.so")
print("mask so: " .. dylib)

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
                            os.execute("kill -KILL " .. t.pid)
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

local masker = compile_mask_so()
local pid_t = create_test_lua()
local pid = pid_t.pid
assert(pid)
print("target pid:", pid)

local function process_inject(process, entry)
    local helper = "publish/bin/process_inject_helper"
    local shell = ([[/usr/bin/osascript -e "do shell script \"%s %d %s %s\" with administrator privileges with prompt \"lua-debug\""]])
        :format(helper, process, dylib, entry)

    return os.execute(shell)
end

if not process_inject then
    process_inject = require("frontend.process_inject").macos_inject
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

WORKDIR = fs.current_path() / "publish"
assert(process_inject(pid, "attach", dylib))
print("process injecte: ", pid)
assert(wait_injected(), "inject failed")
