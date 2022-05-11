local fs = require 'bee.filesystem'
local OS = require 'bee.platform'.OS:lower()

local function getExtensionDirName(packageDir)
    local publisher,name,version
    for line in io.lines(packageDir .. '/package.json') do
        if not publisher then
            publisher = line:match('"publisher": "([^"]+)"')
        end
        if not name then
            name = line:match('"name": "([^"]+)"')
        end
        if not version then
            version = line:match('"version": "(%d+%.%d+%.%d+)"')
        end
    end
    if not publisher then
        error 'Cannot found `publisher` in package.json.'
    end
    if not name then
        error 'Cannot found `name` in package.json.'
    end
    if not version then
        error 'Cannot found `version` in package.json.'
    end
    return ('%s.%s-%s'):format(publisher,name,version)
end

local function crtdll(path)
    if path:find("api-ms-win-", 1, true) then
        return true
    end
    if path:sub(1, 5) == "msvcp" then
        return true
    end
    if path:sub(1, 9) == "vcruntime" then
        return true
    end
    if path == "concrt140.dll" then
        return true
    end
    if path == "ucrtbase.dll" then
        return true
    end
    return false
end

local function copy_directory(from, to, filter)
    fs.create_directories(to)
    for fromfile in fs.pairs(from) do
        if (not filter) or filter(fromfile) then
            if fs.is_directory(fromfile) then
                copy_directory(fromfile, to / fromfile:filename(), filter)
            else
                local filename = fromfile:filename()
                if not crtdll(filename:string():lower()) then
                    print('copy', fromfile, to / filename)
                end
                fs.copy_file(fromfile, to / filename, fs.copy_options.overwrite_existing)
            end
        end
    end
end

local function what_arch()
    if OS == "windows" then
        if os.getenv "PROCESSOR_ARCHITECTURE" == "ARM64" then
            return "arm64"
        end
        if os.getenv "PROCESSOR_ARCHITECTURE" == "AMD64" or os.getenv "PROCESSOR_ARCHITEW6432" == "AMD64" then
            return "x64"
        end
        return "ia32"
    end
    local f <close> = assert(io.popen("uname -m", 'r'))
    return f:read 'l':lower()
end

local function detectPlatform(extensionPath, extensionDirName)
    local extensionDirPrefix = fs.path(extensionPath) / extensionDirName
    local function guess(platform)
        local extensionDir = extensionDirPrefix..platform
        if fs.exists(extensionDir / "package.json") then
            return extensionDir
        end
    end
    local arch = what_arch()
    if OS == "windows" then
        local r = guess('-win32-'..arch)
        if r then return r end
        if arch == "x64" then
            r = guess '-win32-ia32'
            if r then return r end
        end
    elseif OS == "linux" then
        if arch == "x86_64" then
            local r = guess '-linux-x64'
            if r then return r end
        elseif arch == "aarch64" then
            local r = guess '-linux-arm64'
            if r then return r end
        end
    elseif OS == "macos" then
        if arch == "arm64" then
            local r = guess '-darwin-arm64'
            if r then return r end
        end
        local r = guess '-darwin-x64'
        if r then return r end
    end
    local r = guess ''
    if r then return r end
    error("`" .. extensionDirPrefix:string() .. "` is not installed.")
end

local packageDir,sourceDir,extensionPath = ...
local extensionDirName = getExtensionDirName(packageDir)
local extensionDir = detectPlatform(extensionPath, extensionDirName)

local sourceDir = fs.path(sourceDir)
copy_directory(sourceDir, extensionDir, function (path)
    local ext = path:extension():string():lower()
    return ext ~= '.log' and path ~= sourceDir / "tmp"
end)

print 'ok'
