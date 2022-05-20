local lm = require "luamake"

local function windows_arch()
    if os.getenv "PROCESSOR_ARCHITECTURE" == "ARM64" then
        return "arm64"
    end
    if os.getenv "PROCESSOR_ARCHITECTURE" == "AMD64" or os.getenv "PROCESSOR_ARCHITEW6432" == "AMD64" then
        return "x64"
    end
    return "ia32"
end

local function macos_support_arm64()
    local f = io.popen("sw_vers", "r")
    if f then
        for line in f:lines() do
            local major, minor = line:match "ProductVersion:%s*(%d+)%.(%d+)"
            if major and minor then
                return tonumber(major) >= 11
            end
        end
    end
end

local function posix_arch()
    local f <close> = assert(io.popen("uname -m", 'r'))
    return f:read 'l':lower()
end

local function detect_windows()
    local arch = windows_arch()
    assert(arch ~= "arm64")
    if arch == "ia32" then
        if lm.platform then
            assert(lm.platform == "win32-ia32")
        else
            lm.platform = "win32-ia32"
        end
        return
    end
    if lm.platform then
        assert(lm.platform == "win32-ia32" or lm.platform == "win32-x64")
    else
        lm.platform = "win32-x64"
    end
end

local function detect_macos()
    if lm.platform then
        if lm.platform == "darwin-arm64" then
            assert(macos_support_arm64())
        else
            assert(lm.platform == "darwin-x64")
        end
    else
        if posix_arch() == "arm64" then
            lm.platform = "darwin-arm64"
        else
            lm.platform = "darwin-x64"
        end
    end
end

local function detect_linux()
    if lm.platform then
        if lm.platform == "linux-arm64" then
            if posix_arch() ~= "aarch64" then
				lm.cross_compile = true
                lm.cc = "aarch64-linux-gnu-gcc"
            end
        else
            assert(lm.platform == "linux-x64")
        end
    else
        if posix_arch()  == "aarch64" then
            lm.platform = "linux-arm64"
        else
            lm.platform = "linux-x64"
        end
    end
end

local function detect_android()
    if lm.platform then
        assert(lm.platform == "linux-arm64")
    else
        lm.platform = "linux-arm64"
    end
end

if lm.os == "windows" then
    detect_windows()
elseif lm.os == "macos" then
    detect_macos()
elseif lm.os == "linux" then
    detect_linux()
elseif lm.os == "android" then
    detect_android()
else
    error("unknown OS:"..lm.os)
end
