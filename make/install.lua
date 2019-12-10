local platform, luamake = ...

if not platform then
    local OS = require 'bee.platform'.OS
    if OS == 'Windows' then
        platform = 'msvc'
    elseif OS == 'macOS' then
        platform = 'macos'
    elseif OS == 'Linux' then
        platform = 'linux'
    end
end

if not luamake then
    luamake = 'luamake'
end

local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local root = fs.absolute(fs.path '.')

local function spawn(t)
    local process = assert(sp.spawn(t))
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end

if platform == 'msvc' then
    spawn {
        luamake, 'remake', '-f', 'make-runtime.lua', '-arch', 'x86',
        cwd = root,
        searchPath = true,
    }
end

spawn {
    luamake, 'remake', '-f', 'make-runtime.lua', '-arch', 'x64',
    cwd = root,
    searchPath = true,
}
