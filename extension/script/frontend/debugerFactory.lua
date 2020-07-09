local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local platformOS = require 'frontend.platformOS'
local inject = require 'inject'

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
    if args.luaVersion == "5.4" then
        return 54
    elseif args.luaVersion == "5.3" then
        return 53
    elseif args.luaVersion == "5.2" then
        return 52
    elseif args.luaVersion == "5.1" then
        return 51
    end
    return 53
end

local function getLuaExe(args, dbg)
    if type(args.luaexe) == "string" then
        return fs.path(args.luaexe)
    end
    local ver = getLuaVersion(args)
    local runtime = 'runtime'
    if platformOS() == "Windows" then
        if args.luaArch == "x86_64" then
            runtime = runtime .. "/win64"
        else
            runtime = runtime .. "/win32"
        end
    else
        runtime = runtime .. "/" .. platformOS():lower()
    end
    runtime = runtime .. "/lua" .. ver
    return dbg / runtime / (platformOS() == "Windows" and "lua.exe" or "lua")
end

local function installBootstrap1(option, luaexe, args)
    option.cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string()
    if type(args.env) == "table" then
        option.env = args.env
    end
end

local function installBootstrap2(c, luaexe, pid, dbg)
    c[#c+1] = towsl(luaexe:string())
    c[#c+1] = "-e"
    c[#c+1] = ("dofile[[%s]];DBG(%d%s)"):format(
        (dbg / "script" / "launch.lua"):string(),
        pid,
        useUtf8 and "" or ",true"
    )
end

local function installBootstrap2Simple(c, luaexe, pid)
    c[#c+1] = towsl(luaexe:string())
    c[#c+1] = "-ldbg"
    c[#c+1] = "-e"
    c[#c+1] = ("DBG(%d)"):format(pid)
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

local function exists_exe(luaexe, modify)
    if fs.exists(luaexe) then
        return true
    end
    if platformOS() ~= "Windows" then
        return false
    end
    if not luaexe:equal_extension "" then
        return false
    end
    if modify then
        luaexe:replace_extension "exe"
    else
        luaexe = fs.path(luaexe):replace_extension "exe"
    end
    return fs.exists(luaexe)
end

local function create_luaexe_in_terminal(args, dbg, pid)
    initialize(args)
    local luaexe = getLuaExe(args, dbg)
    if not exists_exe(luaexe, false) then
        if args.luaexe then
            return nil, ("No file `%s`."):format(args.luaexe)
        end
        return nil, "Non-Windows need to compile lua-debug first."
    end
    local option = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = "Lua Debug",
        args = {},
    }
    if useWSL then
        option.args[1] = "wsl"
    end
    installBootstrap1(option, luaexe, args)
    if type(args.luaexe) == "string" then
        installBootstrap2(option.args, luaexe, pid, dbg)
    else
        installBootstrap2Simple(option.args, luaexe, pid)
    end
    installBootstrap3(option.args, args)
    return option
end

local function create_luaexe_in_console(args, dbg, pid)
    initialize(args)
    local luaexe = getLuaExe(args, dbg)
    if not exists_exe(luaexe, true) then
        if args.luaexe then
            return nil, ("No file `%s`."):format(args.luaexe)
        end
        return nil, "Non-Windows need to compile lua-debug first."
    end
    local option = {
        console = 'hide'
    }
    if useWSL then
        local SystemRoot = (os.getenv "SystemRoot") or "C:\\WINDOWS"
        option[1] = SystemRoot .. "\\sysnative\\wsl.exe"
    end
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option, luaexe, pid, dbg)
    installBootstrap3(option.args, args)
    return sp.spawn(option)
end

local function create_process_in_console(args)
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
    inject.injectdll(process
        , (WORKDIR / "bin" / "win" / "launcher.x86.dll"):string()
        , (WORKDIR / "bin" / "win" / "launcher.x64.dll"):string()
        , "launch"
    )
    process:resume()
    return process
end

return {
    create_luaexe_in_console   = create_luaexe_in_console,
    create_luaexe_in_terminal  = create_luaexe_in_terminal,
    create_process_in_console  = create_process_in_console,
}
