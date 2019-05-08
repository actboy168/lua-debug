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

local function nativepath(s)
    if not useWSL and not useUtf8 and platformOS() == "Windows" then
        local unicode = require 'bee.unicode'
        return unicode.u2a(s)
    end
    return towsl(s)
end

local function create_install_script(args, pid, dbg)
    local res = {}
    res[#res+1] = ("local path=[[%s]];"):format(nativepath(dbg))
    if type(args.internalModule) == "string" then
        res[#res+1] = ("assert(loadfile(path..'/script/debugger.lua'))('%s',path,%d,[[%s]])"):format(
            platformOS():lower(),
            pid,
            args.internalModule
        )
    else
        res[#res+1] = ("assert(loadfile(path..'/script/debugger.lua'))('%s',path,%d)"):format(
            platformOS():lower(),
            pid
        )
    end
    return table.concat(res)
end

local function is64Exe(exe)
    local f = io.open(exe:string())
    if not f then
        return
    end
    local MZ = f:read(2)
    if MZ ~= 'MZ' then
        f:close()
        return
    end
    f:seek('set', 60)
    local e_lfanew = ('I4'):unpack(f:read(4))
    f:seek('set', e_lfanew)
    local ntheader = ('z'):unpack(f:read(4) .. '\0')
    if ntheader ~= 'PE' then
        f:close()
        return
    end
    f:seek('cur', 18)
    local characteristics = ('I2'):unpack(f:read(2))
    f:close()
    return (characteristics & 0x100) == 0
end

local function getLuaRuntime(args)
    if args.luaRuntime == "5.4 64bit" then
        return 54, 64
    elseif args.luaRuntime == "5.4 32bit" then
        return 54, 32
    elseif args.luaRuntime == "5.3 64bit" then
        return 53, 64
    elseif args.luaRuntime == "5.3 32bit" then
        return 53, 32
    end
    return 53, 32
end

local function getLuaExe(args, dbg)
    if type(args.luaexe) == "string" then
        return fs.path(args.luaexe)
    end
    local ver, bit = getLuaRuntime(args)
    local runtime = 'runtime'
    if platformOS() == "Windows" then
        if bit == 64 then
            runtime = runtime .. "/win64"
        else
            runtime = runtime .. "/win32"
        end
    else
        runtime = runtime .. "/" .. platformOS():lower()
    end
    if ver == 53 then
        runtime = runtime .. "/lua53"
    else
        runtime = runtime .. "/lua54"
    end
    return dbg / runtime / (platformOS() == "Windows" and "lua.exe" or "lua")
        , ver
end

local function installBootstrap1(option, luaexe, args)
    option.cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string()
    if type(args.env) == "table" then
        option.env = args.env
    end
end

local function installBootstrap2(c, luaexe, args, pid, dbg)
    if useWSL then
        c[#c+1] = "wsl"
    end
    c[#c+1] = towsl(luaexe:string())
    c[#c+1] = "-e"
    c[#c+1] = create_install_script(args, pid, dbg:string())

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

local function create_terminal(args, dbg, pid)
    initialize(args)
    local luaexe = getLuaExe(args, dbg)
    if not fs.exists(luaexe) then
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
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option.args, luaexe, args, pid, dbg)
    return option
end

local function create_luaexe(args, dbg, pid)
    initialize(args)
    local luaexe, ver = getLuaExe(args, dbg)
    if not fs.exists(luaexe) then
        if args.luaexe then
            return nil, ("No file `%s`."):format(args.luaexe)
        end
        return nil, "Non-Windows need to compile lua-debug first."
    end
    local option = {
        console = 'hide'
    }
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option, luaexe, args, pid, dbg)
    if not args.luadll or not ver or platformOS() ~= "Windows" then
        return sp.spawn(option)
    end
    option.suspended = true
    local process, err = sp.spawn(option)
    if not process then
        return nil, err
    end
    --TODO: Not dependent on luaRuntime
    inject.replacedll(process
        , ver == 53 and "lua53.dll" or "lua54.dll"
        , args.luadll
    )
    process:resume()
    return process
end

local function create_process(args)
    initialize(args)
    local application = args.runtimeExecutable
    local option = {
        application,
        env = args.env,
        console = 'new',
        cwd = args.cwd or fs.path(application):parent_path(),
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
    create_terminal = create_terminal,
    create_process = create_process,
    create_luaexe = create_luaexe,
}
