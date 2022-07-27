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
    elseif args.luaVersion == "jit" then
        return "luajit"
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

local PLATFORM = {
    ["windows-x86"]    = "win32-ia32",
    ["windows-x86_64"] = "win32-x64",
    ["linux-x86_64"]   = "linux-x64",
    ["linux-arm64"]    = "linux-arm64",
    ["android-arm64"]  = "linux-arm64",
    ["macos-x86_64"]   = "darwin-x64",
    ["macos-arm64"]    = "darwin-arm64",
}

local function getLuaExe(args, dbg)
    local OS = platform_os():lower()
    local ARCH = args.luaArch
    if OS == "windows" then
        ARCH = ARCH or "x86_64"
        if ARCH == "x86_64" and not Is64BitWindows() then
            ARCH = "x86"
        end
    elseif OS == "linux" then
        ARCH = ARCH or "x86_64"
    elseif OS == "macos" then
        if IsArm64Macos() then
            ARCH = ARCH or "x86_64"
            if ARCH == "x86" then
                ARCH = "x86_64"
            end
        else
            ARCH = "x86_64"
        end
    elseif OS == "android" then
        ARCH = "arm64"
    end
    local platform = PLATFORM[OS.."-"..ARCH]
    if platform then
        local luaexe = dbg / "runtime"
            / platform
            / getLuaVersion(args)
            / (OS == "windows" and "lua.exe" or "lua")
        if fs.exists(luaexe) then
            return luaexe
        end
    end
    return nil, ("No runtime (OS: %s, ARCH: %s) is found, you need to compile it yourself."):format(OS, ARCH)
end

local function bootstrapOption(option, luaexe, args)
    option.cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string()
    if type(args.env) == "table" then
        option.env = args.env
    end
end

local function bootstrapMakeExe(c, luaexe, args, address, dbg)
    c[#c+1] = towsl(luaexe:string())
    c[#c+1] = "-e"
    local params = {}
    params[#params+1] = address
    if not useUtf8 then
        params[#params+1] = 'ansi'
    end
    if args.luaVersion == "latest" then
        params[#params+1] = 'latest'
    end
    local script = ("dofile[[%s]];DBG[[%s]]"):format(
        (dbg / "script" / "launch.lua"):string(),
        table.concat(params, "-")
    )
    local bash = platform_os():lower() ~= "windows"
    if bash then
        script = script:gsub('%[%[', '"'):gsub('%]%]', '"')
    end
    c[#c+1] = script
end

local function bootstrapMakeArgs(c, args)
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
        if not args.luaexe:find(package.config:sub(1,1), 1, true) then
            return luaexe
        end
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

local function create_luaexe_in_terminal(_, args, dbg, address)
    initialize(args)
    local luaexe, err = checkLuaExe(args, dbg)
    if not luaexe then
        return nil, err
    end
    local option = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = args.name,
        args = {},
        --TODO: support argsCanBeInterpretedByShell
    }
    if useWSL then
        option.args[1] = "wsl"
    end
    bootstrapOption(option, luaexe, args)
    bootstrapMakeExe(option.args, luaexe, args, address, dbg)
    bootstrapMakeArgs(option.args, args)
    return option
end

local function create_luaexe_in_console(args, dbg, address)
    initialize(args)
    local luaexe, err = checkLuaExe(args, dbg)
    if not luaexe then
        return nil, err
    end
    local option = {
        console = 'hide',
        searchPath = true,
    }
    if useWSL then
        local SystemRoot = (os.getenv "SystemRoot") or "C:\\WINDOWS"
        option[1] = SystemRoot .. "\\sysnative\\wsl.exe"
    end
    bootstrapOption(option, luaexe, args)
    bootstrapMakeExe(option, luaexe, args, address, dbg)
    bootstrapMakeArgs(option, args)
    return sp.spawn(option)
end

local function create_process_in_console(args, callback)
    initialize(args)
    local process, err = sp.spawn {
        args.runtimeExecutable, args.runtimeArgs,
        env = args.env,
        console = 'new',
        cwd = args.cwd or fs.path(args.runtimeExecutable):parent_path(),
        suspended = true,
    }
    if not process then
        return nil, err
    end
    if args.inject == "hook" then
        local inject = require 'inject'
        inject.injectdll(process
            , (WORKDIR / "bin" / "launcher.x86.dll"):string()
            , (WORKDIR / "bin" / "launcher.x64.dll"):string()
            , "launch"
        )
    end
    if callback then
        callback(process)
    end
    process:resume()
    return process
end

local function create_process_in_terminal(client, args)
    initialize(args)
    local arguments = {}
    if useWSL then
        arguments[#arguments+1] = "wsl"
    end
    arguments[#arguments+1] = args.runtimeExecutable
    if type(args.runtimeArgs) == "string" then
        arguments[#arguments+1] = args.runtimeArgs
    elseif type(args.runtimeArgs) == "table" then
        for _, v in ipairs(args.runtimeArgs) do
            arguments[#arguments+1] = v
        end
    end
    local option = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = args.name,
        env = args.env,
        cwd = args.cwd or fs.path(args.runtimeExecutable):parent_path(),
        args = arguments,
        argsCanBeInterpretedByShell = client.arguments.supportsArgsCanBeInterpretedByShell and type(args.runtimeArgs) == "string",
    }
    return option
end

return {
    create_luaexe_in_console   = create_luaexe_in_console,
    create_luaexe_in_terminal  = create_luaexe_in_terminal,
    create_process_in_console  = create_process_in_console,
    create_process_in_terminal  = create_process_in_terminal,
}
