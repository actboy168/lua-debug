local unicode = require 'bee.unicode'
local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local inject = require 'inject'

local function create_install_script(args, dbg, port)
    local utf8 = args.sourceCoding == "utf8"
    local res = {}
    if type(args.path) == "string" then
        res[#res+1] = ("package.path=[[%s]];"):format(utf8 and args.path or unicode.u2a(args.path))
    elseif type(args.path) == "table" then
        local path = {}
        for _, v in ipairs(args.path) do
            if type(v) == "string" then
                path[#path+1] = utf8 and v or unicode.u2a(v)
            end
            res[#res+1] = ("package.path=[[%s]];"):format(table.concat(path, ";"))
        end
    end
    if type(args.cpath) == "string" then
        res[#res+1] = ("package.cpath=[[%s]];"):format(utf8 and args.cpath or unicode.u2a(args.cpath))
    elseif type(args.cpath) == "table" then
        local path = {}
        for _, v in ipairs(args.cpath) do
            if type(v) == "string" then
                path[#path+1] = utf8 and v or unicode.u2a(v)
            end
            res[#res+1] = ("package.cpath=[[%s]];"):format(table.concat(path, ";"))
        end
    end

    res[#res+1] = ("local dbg=package.loadlib([[%s]], 'luaopen_debugger')();package.loaded[ [[%s]] ]=dbg;dbg:io([[pipe:%s]])"):format(
        utf8 and (dbg / "debugger.dll"):string() or unicode.u2a((dbg / "debugger.dll"):string()),
        (type(args.internalModule) == "string") and args.internalModule or "debugger",
        port
    )

    if type(args.outputCapture) == "table" then
        for _, v in ipairs(args.outputCapture) do
            if type(v) == "string" then
                res[#res+1] = (":redirect('%s')"):format(v);
            end
        end
    end
    res[#res+1] = ":guard():wait():start()"
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
    dbg = dbg / "windows"

    local luaexe
    if type(args.luaexe) == "string" then
        luaexe = fs.path(args.luaexe)
        if is64Exe(luaexe) then
            dbg = dbg / "x64"
        else
            dbg = dbg / "x86"
        end
    else
        local ver, bit = getLuaRuntime(args)
        if bit == 64 then
            dbg = dbg / "x64"
        else
            dbg = dbg / "x86"
        end
        if ver == 53 then
            luaexe = dbg / "lua53.exe"
        else
            luaexe = dbg / "lua54.exe"
        end
    end
    return luaexe, dbg
end

local function installBootstrap1(option, luaexe, args)
    option.cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string()
    if type(args.env) == "table" then
        option.env = args.env
    end
end

local function installBootstrap2(c, luaexe, args, dbgdir, port)
    c[#c+1] = luaexe:string()
    c[#c+1] = "-e"
    c[#c+1] = create_install_script(args, dbgdir, port)

    if type(args.arg0) == "string" then
        c[#c+1] = args.arg0
    elseif type(args.arg0) == "table" then
        for _, v in ipairs(args.arg0) do
            if type(v) == "string" then
                c[#c+1] = v
            end
        end
    end

    c[#c+1] = (type(args.program) == "string") and args.program or ".lua"

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

local function create_terminal(args, dbg, port)
    local luaexe, dbgdir = getLuaExe(args, dbg)
    local option = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = "Lua Debug",
        args = {},
    }
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option.args, luaexe, args, dbgdir, port)
    return option
end

local function create_luaexe(args, dbg, port)
    local luaexe, dbgdir = getLuaExe(args, dbg)
    local option = {
        console = 'hide'
    }
    installBootstrap1(option, luaexe, args)
    installBootstrap2(option, luaexe, args, dbgdir, port)
    if not args.luadll or type(args.luaexe) == "string" then
        return sp.spawn(option)
    end
    option.suspended = true
    local process, err = sp.spawn(option)
    if not process then
        return process, err
    end
    inject.replacedll(process
        , getLuaRuntime(args) == 53 and "lua53.dll" or "lua54.dll"
        , args.luadll
    )
    process:resume()
    return process
end

local function create_process(args)
    local noinject = args.noInject
    local application = args.runtimeExecutable
    local option = {
        application,
        env = args.env,
        console = 'new',
        cwd = args.cwd or fs.path(application):parent_path(),
        suspended = not noinject,
    }
    local process
    if type(args.runtimeArgs) == 'string' then
        option.argsStyle = 'string'
        option[2] = args.runtimeArgs
        process = sp.spawn(option)
    elseif type(args.runtimeArgs) == 'table' then
        option[2] = args.runtimeArgs
        process = sp.spawn(option)
    else
        process = sp.spawn(option)
    end
    if noinject then
        return process
    end
    inject.injectdll(process
        , (WORKDIR / "windows" / "x86" / "debugger-inject.dll"):string()
        , (WORKDIR / "windows" / "x64" / "debugger-inject.dll"):string()
    )
    process:resume()
    return process
end

return {
    create_terminal = create_terminal,
    create_process = create_process,
    create_luaexe = create_luaexe,
}
