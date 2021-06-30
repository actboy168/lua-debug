local network = require 'common.network'
local debuger_factory = require 'frontend.debuger_factory'
local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local platform_os = require 'frontend.platform_os'
local server
local client
local initReq
local m = {}

local function getUnixAddress(pid)
    local path = WORKDIR / "tmp"
    fs.create_directories(path)
    return "@"..(path / ("pid_%d"):format(pid)):string()
end

local function ipc_send_latest(pid)
    fs.create_directories(WORKDIR / "tmp")
    local ipc = require "common.ipc"
    local fd = assert(ipc(WORKDIR, pid, "luaVersion", "w"))
    fd:write("latest")
    fd:close()
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
    if pkg.arguments.luaVersion == "latest" then
        ipc_send_latest(pid)
    end
    local inject = require 'inject'
    if not inject.injectdll(pid
        , (WORKDIR / "bin" / "windows" / "launcher.x86.dll"):string()
        , (WORKDIR / "bin" / "windows" / "launcher.x64.dll"):string()
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
    platform_os.init(args)
    if platform_os() ~= "Windows" then
        attach_tcp(pkg, args)
        return
    end
    if args.processId then
        local processId = tonumber(args.processId)
        if not attach_process(pkg, processId) then
            response_error(pkg, ('Cannot attach process `%d`.'):format(processId))
        end
        return
    end
    if args.processName then
        local pids = require "frontend.query_process"(args.processName)
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
        response_error(pkg, "`runtimeExecutable` is not supported in `"..args.console.."`.")
        return
    end
    local pid = sp.get_id()
    server = network(getUnixAddress(pid), true)
    local arguments, err = debuger_factory.create_luaexe_in_terminal(args, WORKDIR, pid)
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
        if platform_os() ~= "Windows" then
            response_error(pkg, "`runtimeExecutable` is not supported.")
            return
        end
        local process, err = debuger_factory.create_process_in_console(args, function (process)
            local pid = process:get_id()
            server = network(getUnixAddress(pid), true)
            if args.luaVersion == "latest" then
                ipc_send_latest(pid)
            end
        end)
        if not process then
            response_error(pkg, err)
            return
        end
    else
        local pid = sp.get_id()
        server = network(getUnixAddress(pid), true)
        local ok, err = debuger_factory.create_luaexe_in_console(args, WORKDIR, pid)
        if not ok then
            response_error(pkg, err)
            return
        end
    end
    return true
end

local function proxy_launch(pkg)
    local args = pkg.arguments
    platform_os.init(args)
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
    if pkg.arguments.request == 'attach' then
        proxy_attach(pkg)
    elseif pkg.arguments.request == 'launch' then
        proxy_launch(pkg)
    end
end

function m.send(pkg)
    if server then
        if pkg.type == 'response' and pkg.command == 'runInTerminal' then
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
