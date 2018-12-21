local unicode = require 'bee.unicode'

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
    -- TODO
    return false
end

local function getLuaRuntime(args)
    if args.luaRuntime == "5.4 64bit" then
        return 54, 64
    elseif luaRuntime == "5.4 32bit" then
        return 54, 32
    elseif luaRuntime == "5.3 64bit" then
        return 53, 64
    elseif luaRuntime == "5.3 32bit" then
        return 53, 32
    end
    return 53, 32
end

local function create_terminal(args, dbg, port)
    dbg = dbg / "windows"

    local luaexe
    if type(args.luaexe) == "string" then
        luaexe = args.luaexe
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
    local res = {
        kind = (args.console == "integratedTerminal") and "integrated" or "external",
        title = "Lua Debug",
        cwd = (type(args.cwd) == "string") and args.cwd or luaexe:parent_path():string(),
        args = {},
    }

    if type(args.env) == "table" then
        -- TODO: json null ?
        res.env = args.env
    end

    local c = res.args
    c[#c+1] = luaexe:string()
    c[#c+1] = "-e"
    c[#c+1] = create_install_script(args, dbg, port)

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

    return res
end


return {
    create_terminal = create_terminal,
}
