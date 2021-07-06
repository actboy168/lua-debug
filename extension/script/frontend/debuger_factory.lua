local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local platform_os = require 'frontend.platform_os'

local useWSL = false
local useUtf8 = false

local function initialize(args)
    useWSL = args.useWSL
    useUtf8 = args.sourceCoding == "utf8"
end

local function towsl(s)
    if not useWSL or not s:match "^%a:" then
        return s
    end
    return s:gsub("\\", "/"):gsub("^(%a):", function(c)
        return "/mnt/"..c:lower()
    end)
end

local function getLuaVersion(args)
    if args.luaVersion == "latest" then
        return "lua-latest"
    elseif args.luaVersion == "5.4" then
        return "lua54"
    elseif args.luaVersion == "5.3" then
        return "lua53"
    elseif args.luaVersion == "5.2" then
        return "lua52"
    elseif args.luaVersion == "5.1" then
        return "lua51"
    end
    return "lua54"
end

local function Is64BitWindows()
    -- https://docs.microsoft.com/en-us/archive/blogs/david.wang/howto-detect-process-bitness
    return os.getenv "PROCESSOR_ARCHITECTURE" == "AMD64" or os.getenv "PROCESSOR_ARCHITEW6432" == "AMD64"
end

local function IsArm64Macos()
    local f <close> = assert(io.popen("uname -v", "r"))
    if f:read "l":match "RELEASE_ARM64" then
        return true
    end
end

local function getLuaExe(args, dbg)
    local OS = platform_os():lower()
    local ARCH = args.luaArch
    if OS == "windows" then
        if ARCH == "x86_64" and not Is64BitWindows() then
            ARCH = "x86"
        end
    elseif OS == "linux" then
        ARCH = "x86_64"
    elseif OS == "macos" then
        if IsArm64Macos() then
            if ARCH == "x86" then
                ARCH = "x86_64"
            end
        else
            ARCH = "x86_64"
        end
    end
    local luaexe = dbg / "runtime"
        / OS
        / ARCH
        / getLuaVersion(args)
        / (OS == "windows" and "lua.exe" or "lua")
    if fs.exists(luaexe) then
        return luaexe
    end
    return nil, ("No runtime (OS: %s, ARCH: %s) is found, you need to compile it yourself."):format(OS, ARCH)
end

local function installBootstrap1(option, luaexe, args)
    option.cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string()
    if type(args.env) == "table" then
        option.env = args.env
    end
end

local function installBootstrap2(c, luaexe, args, pid, dbg)
    c[#c+1] = towsl(luaexe:string())
    c[#c+1] = "-e"
    local params = {}
    params[#params+1] = pid
    if not useUtf8 then
        params[#params+1] = '[[ansi]]'
    end
    if args.luaVersion == "latest" then
        params[#params+1] = '[[latest]]'
    end
    c[#c+1] = ("dofile[[%s]];DBG{%s}"):format(
        (dbg / "script" / "launch.lua"):string(),
        table.concat(params, ",")
    )
end

local function installBootstrap3(c, args)
    if type(args.arg0) == "string" then
        c[#c+1] = args.arg0
    elseif type(args.arg0) == "table" then
        for _, v in ipairs(args.arg0) do
            if type(v) == "string" then
                c[#c+1] = v
            end
        end
    end

    c[#c+1] = (type(args.program) == "string") and towsl(args.program) or ".lua"

    if type(args.arg) == "string" then
        c[#c+1] = args.arg
    elseif type(args.arg) == "table" then
        for _, v in ipairs(args.arg) do
            if type(v) == "string" then
                c[#c+1] = v
            end
        end
    end
end

local function checkLuaExe(args, dbg)
    if type(args.luaexe) == "string" then
        local luaexe = fs.path(args.luaexe)
        if fs.exists(luaexe) then
            return luaexe
        end
        if platform_os() == "Windows" and luaexe:equal_extension "" then
            luaexe = fs.path(luaexe):replace_extension "exe"
            if fs.exists(luaexe) then
                return luaexe
            end
        end
        return nil, ("No file `%s`."):format(args.luaexe)
    end
    return getLuaExe(args, dbg)
end

local function create_luaexe_in_terminal(args, dbg, pid)
    initialize(args)
    local luaexe, err = checkLuaExe(args, dbg)
    if not luaexe then
        return nil, err
    end
    local option = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = args.name,
        args = {},
    }
    if useWSL then
        option.args[1] = "wsl"
    end
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option.args, luaexe, args, pid, dbg)
    installBootstrap3(option.args, args)
    return option
end

local function create_luaexe_in_console(args, dbg, pid)
    initialize(args)
    local luaexe, err = checkLuaExe(args, dbg)
    if not luaexe then
        return nil, err
    end
    local option = {
        console = 'hide'
    }
    if useWSL then
        local SystemRoot = (os.getenv "SystemRoot") or "C:\\WINDOWS"
        option[1] = SystemRoot .. "\\sysnative\\wsl.exe"
    end
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option, luaexe, args, pid, dbg)
    installBootstrap3(option, args)
    return sp.spawn(option)
end

local function create_process_in_console(args, callback)
    initialize(args)
    local application = args.runtimeExecutable
    local option = {
        application,
        env = args.env,
        console = 'new',
        cwd = args.cwd or fs.path(application):parent_path(),
        suspended = true,
    }
    local process, err
    if type(args.runtimeArgs) == 'string' then
        option.argsStyle = 'string'
        option[2] = args.runtimeArgs
        process, err = sp.spawn(option)
    elseif type(args.runtimeArgs) == 'table' then
        option[2] = args.runtimeArgs
        process, err = sp.spawn(option)
    else
        process, err = sp.spawn(option)
    end
    if not process then
        return nil, err
    end
    local inject = require 'inject'
    inject.injectdll(process
        , (WORKDIR / "bin" / "windows" / "launcher.x86.dll"):string()
        , (WORKDIR / "bin" / "windows" / "launcher.x64.dll"):string()
        , "launch"
    )
    if callback then
        callback(process)
    end
    process:resume()
    return process
end

return {
    create_luaexe_in_console   = create_luaexe_in_console,
    create_luaexe_in_terminal  = create_luaexe_in_terminal,
    create_process_in_console  = create_process_in_console,
}
