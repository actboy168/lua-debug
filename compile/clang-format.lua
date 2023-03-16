local fs = require "bee.filesystem"
local sp = require "bee.subprocess"

local sourcefile = {}

local EXTENSION <const> = {
    [".h"] = true,
    [".inl"] = true,
    [".c"] = true,
    [".cpp"] = true,
}

local function scan(dir)
    for path in fs.pairs(dir) do
        if fs.is_directory(path) then
            scan(path)
        else
            local ext = path:extension():string():lower()
            if EXTENSION[ext] then
                sourcefile[#sourcefile + 1] = path:string()
            end
        end
    end
end

scan "src"

if #sourcefile > 0 then
    local process = assert(sp.spawn {
        "luamake", "shell", "clang-format",
        "-i", sourcefile,
        stdout = true,
        stderr = "stdout",
        searchPath = true,
    })
    for line in process.stdout:lines() do
        io.write(line, "\n")
        io.flush()
    end
    process.stdout:close()
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end
