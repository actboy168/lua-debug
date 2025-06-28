local fs = require 'bee.filesystem'
local home = assert(os.getenv("HOME") or os.getenv("HOMEPATH"), "HOME environment variable is not set")

local vscode_dir = {
    ".vscode",
    ".vscode-server",
}

local sourceDir = ...

for i, v in ipairs(vscode_dir) do
    local vscode_dir = fs.path(home) / v / "extensions"
    if not fs.exists(vscode_dir) or not fs.is_directory(vscode_dir) then
        goto continue
    end
    local extensions_json = vscode_dir / "extensions.json"
    if not fs.exists(extensions_json) or not fs.is_regular_file(extensions_json) then
        goto continue
    end
    local fp = io.open(extensions_json:string(), "r")
    local content = fp:read("*a")
    fp:close()
    if not content:find("lua-debug", 1, true) then
        goto continue
    end
    for dir in fs.pairs(vscode_dir) do
        if dir:string():find("lua-debug", 1, true) then
            local fn = assert(loadfile('compile/copy.lua'))
            fn(sourceDir or 'publish', dir:string())
            os.exit(0)
        end
    end
    ::continue::
end
os.exit(1)