local network = require 'frontend.network'
local debuggerFactory = require 'frontend.debugerFactory'
local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local platformOS = require 'frontend.platformOS'
local inject = require 'inject'
local server
local client
local initReq
local startReq
local restart = false
local m = {}

local function getDbgPath()
    if WORKDIR:filename():string() ~= 'extension' then
        return WORKDIR
    end
    return arg[2]
end

local function getUnixAddress(pid)
    local path = getDbgPath() / "tmp"
    fs.create_directories(path)
    return "@"..(path / ("pid_%d"):format(pid)):string()
end

local function response_initialize(req)
    client.sendmsg {
        type = 'response',
        seq = 0,
        command = 'initialize',
        request_seq = req.seq,
        success = true,
        body = require 'common.capabilities',
    }
end

local function response_restart(req)
    client.sendmsg {
        type = 'response',
        seq = 0,
        command = 'restart',
        request_seq = req.seq,
        success = true,
    }
end

local function response_error(req, msg)
    client.sendmsg {
        type = 'response',
        seq = 0,
        command = req.command,
        request_seq = req.seq,
        success = false,
        message = msg,
    }
end

local function request_runinterminal(args)
    client.sendmsg {
        type = 'request',
        seq = 0,
        command = 'runInTerminal',
        arguments = args
    }
end

local function attach_process(pkg, pid)
    if not inject.injectdll(pid
        , (WORKDIR / "bin" / "win" / "launcher.x86.dll"):string()
        , (WORKDIR / "bin" / "win" / "launcher.x64.dll"):string()
        , "attach"
    ) then
        return false
    end
    server = network(getUnixAddress(pid), true)
    server.sendmsg(initReq)
    server.sendmsg(pkg)
    return true
end

local function attach_tcp(pkg, args)
    server = network(args.address, args.client)
    server.sendmsg(initReq)
    server.sendmsg(pkg)
end

local function proxy_attach(pkg)
    local args = pkg.arguments
    platformOS.init(args)
    if platformOS() ~= "Windows" then
        attach_tcp(pkg, args)
        return
    end
    if args.processId then
        if not attach_process(pkg, args.processId) then
            response_error(pkg, ('Cannot attach process `%d`.'):format(args.processId))
        end
        return
    end
    if args.processName then
        local pids = inject.query_process(args.processName)
        if #pids == 0 then
            response_error(pkg, ('Cannot found process `%s`.'):format(args.processName))
            return
        elseif #pids > 1 then
            response_error(pkg, ('There are %d processes `%s`.'):format(#pids, args.processName))
            return
        end
        if not attach_process(pkg, pids[1]) then
            response_error(pkg, ('Cannot attach process `%s` `%d`.'):format(args.processName, pids[1]))
        end
        return
    end
    attach_tcp(pkg, args)
end

local function proxy_launch_terminal(pkg)
    local args = pkg.arguments
    if args.runtimeExecutable then
        --TODO: support runtimeExecutable's integratedTerminal/externalTerminal
        response_error(pkg, "`runtimeExecutable` is not supported.")
        return
    end
    local pid = sp.get_id()
    server = network(getUnixAddress(pid), true)
    local arguments, err = debuggerFactory.create_terminal(args, getDbgPath(), pid)
    if not arguments then
        response_error(pkg, err)
        return
    end
    request_runinterminal(arguments)
    return true
end

local function proxy_launch_console(pkg)
    local args = pkg.arguments
    if args.runtimeExecutable then
        if platformOS() ~= "Windows" then
            response_error(pkg, "`runtimeExecutable` is not supported.")
            return
        end
        local process, err = debuggerFactory.create_process(args)
        if not process then
            response_error(pkg, err)
            return
        end
        server = network(getUnixAddress(process:get_id()), true)
    else
        local pid = sp.get_id()
        server = network(getUnixAddress(pid), true)
        local ok, err = debuggerFactory.create_luaexe(args, getDbgPath(), pid)
        if not ok then
            response_error(pkg, err)
            return
        end
    end
    return true
end

local function proxy_launch(pkg)
    local args = pkg.arguments
    platformOS.init(args)
    if args.console == 'integratedTerminal' or args.console == 'externalTerminal' then
        if not proxy_launch_terminal(pkg) then
            return
        end
    else
        if not proxy_launch_console(pkg) then
            return
        end
    end
    server.sendmsg(initReq)
    server.sendmsg(pkg)
end

local function proxy_start(pkg)
    if pkg.command == 'attach' then
        proxy_attach(pkg)
    elseif pkg.command == 'launch' then
        proxy_launch(pkg)
    end
end

function m.send(pkg)
    if server then
        if pkg.type == 'response' and pkg.command == 'runInTerminal' then
            return
        end
        if pkg.type == 'request' and pkg.command == 'restart' then
            response_restart(pkg)
            server.sendmsg {
                type = 'request',
                seq = 0,
                command = 'terminate',
                arguments = {
                    restart = true,
                }
            }
            restart = true
            return
        end
        server.sendmsg(pkg)
    elseif not initReq then
        if pkg.type == 'request' and pkg.command == 'initialize' then
            pkg.__norepl = true
            initReq = pkg
            response_initialize(pkg)
        else
            response_error(pkg, 'not initialized')
        end
    else
        if pkg.type == 'request' then
            if pkg.command == 'attach' or pkg.command == 'launch' then
                startReq = pkg
                proxy_start(pkg)
            else
                response_error(pkg, 'error request')
            end
        end
    end
end

function m.update()
    if server then
        server.event_close(function()
            if restart then
                restart = false
                proxy_start(startReq)
                return
            end
            os.exit(0, true)
        end)
        while true do
            local pkg = server.recvmsg()
            if pkg then
                client.sendmsg(pkg)
            else
                break
            end
        end
    end
end

function m.init(io)
    client = io
end

return m
