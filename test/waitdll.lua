local platform = require "bee.platform"
local fs = require "bee.filesystem"
local sp = require "bee.subprocess"

assert(platform.os == "macos")
local runtime_platform = "darwin-"..(platform.Arch == "x86_64" and "x64" or platform.Arch)
local lua_path = (fs.path("publish/runtime/") / runtime_platform / "lua51" / "lua"):string()
local launcher_path = "publish/bin/launcher.so"

local executable = fs.path("build") / runtime_platform / "debug" / "bin" / "testwaitdll"

print("testwaitdll:", executable, launcher_path, lua_path)
local proc, err = sp.spawn({
    executable,
    launcher_path,
    lua_path,
})
assert(proc, err)

assert(proc:wait() == 0)
