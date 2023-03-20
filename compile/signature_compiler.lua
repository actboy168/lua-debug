local fs = require "bee.filesystem"
local sp = require "bee.subprocess"
local platform = require "bee.platform"

package.path = package.path..";3rd/json.lua/?.lua"

local json = require "json-beautify"

local luadebug_files = {}

local function scan(dir, platform_arch)
    for path in fs.pairs(dir) do
        if fs.is_directory(path) then
            scan(path, platform_arch or path:filename():string())
        else
            if path:filename():string():find("luadebug") then
                local t = { platform_arch = platform_arch, luadebug = path:string() }

                local dir_path = path:parent_path()
                local version = dir_path:filename():string()
                t.version = version

                if platform.os == "windows" then
                    t.lua_modlue = dir_path / version..".dll"
                else
                    t.lua_modlue = dir_path / "lua"
                end
                luadebug_files[#luadebug_files+1] = t
            end
        end
    end
end

scan((fs.path("publish") / "runtime"):string())

local option = {
    ext = "so",
    is_export = false,
}
if platform.os == "windows" then
    option = {
        ext = "dll",
        is_export = true
    }
end

local launcher_path = (fs.path("publish") / "bin" / "launcher."..option.ext):string()

local function compiler(executable, import_file, is_string, lua_modlue)
    print(executable, import_file, is_string, lua_modlue, option.is_export)
    local process = assert(sp.spawn {
        executable,
        import_file,
        is_string and "true" or "false",
        lua_modlue,
        option.is_export and "true" or "false",
        stdout = true,
    })
    local res = {}
    for line in process.stdout:lines() do
        local name, pattern, offset, hit_offset = line:match([[(.+):([0-9a-f%?]+)%(([%-%+]?%d+)%)@([%-%+]?%d+)]])
        offset = offset ~= "0" and tonumber(offset) or nil
        hit_offset = hit_offset ~= "0" and tonumber(hit_offset) or nil
        res[#res+1] = {
            name = name,
            pattern = pattern,
            offset = offset,
            hit_offset = hit_offset,
        }
    end
    process.stdout:close()

    local code = process:wait()
    if code ~= 0 then
        print("signature_compiler error", code)
        os.exit(code, true)
    end

    return res
end

local output_dir = fs.path("publish") / "signature"

local function get_executable(platform_arch)
    local bin_dir = fs.path("build") / platform_arch

    local modes = {
        "release",
        "debug",
    }
    for _, mode in ipairs(modes) do
        local executable = bin_dir / mode / "bin" / "signature_compiler"
        if fs.exists(executable) then
            return executable:string()
        end
    end
    print("can't find signature_compiler")
    os.exit(1, true)
end

for _, t in ipairs(luadebug_files) do
    local output_dir = output_dir / t.platform_arch
    fs.create_directories(output_dir)
    local lua_modlue = t.lua_modlue
    local executable = get_executable(t.platform_arch)
    local t1 = compiler(executable, launcher_path, true, lua_modlue)
    local t2 = compiler(executable, t.luadebug, false, lua_modlue)
    local signatures = table.pack(table.unpack(t1), table.unpack(t2))
    local output = {}
    for _, signature in ipairs(signatures) do
        output[signature.name] = {
            pattern = signature.pattern,
            offset = signature.offset,
            hit_offset = signature.hit_offset,
        }
    end
    local file = io.open((output_dir / (t.version..".json")):string(), "w")
    file:write(json.beautify(output))
    file:close()
end
