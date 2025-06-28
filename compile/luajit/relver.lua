local fs = require 'bee.filesystem'
local subprocess = require 'bee.subprocess'
local platform = require 'bee.platform'

local luajitDir = arg[1]
local output = arg[2]
---@type bee.subprocess.spawn.options
local args = fs.exists(luajitDir.."/.git") and {
    "git",
    "show",
    "-s",
    "--format=%ct"
} or {
    platform.os ~= "windows" and "cat" or "type",
    luajitDir.."/.relver"
}
args.cwd = luajitDir
args.stdout = true
args.stderr = true
args.searchPath = true

local proc, err = subprocess.spawn(args)
if not proc then
    print(err)
    os.exit(1)
end

proc:wait()

io.open(output, 'w+'):write(proc.stdout:read('a'))
