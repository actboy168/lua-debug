local serverFactory = require 'serverFactory'
local proto = require 'protocol'
local debuggerFactory = require 'debugerFactory'
local server
local client = {}
local seq = 0
local initReq
local m = {}
local io

function client.send(pkg)
    io.send(pkg)
end

local function newSeq()
    seq = seq + 1
    return seq
end

local function response_initialize(req)
    client.send {
        type = 'response',
        seq = newSeq(),
        command = 'initialize',
        request_seq = req.seq,
        success = true,
        body = require 'capabilities',
    }
end

local function response_error(req, msg)
    client.send {
        type = 'response',
        seq = newSeq(),
        command = req.command,
        request_seq = req.seq,
        success = false,
        message = msg,
    }
end

local function request_runinterminal(args)
    client.send {
        type = 'request',
        --seq = newSeq(),
        command = 'runInTerminal',
        arguments = args
    }
end

local function create_terminal(args, port)
    local arguments = debuggerFactory.create_terminal(args, port)
    if not arguments then
        return
    end
    client.send {
        type = 'request',
        --seq = newSeq(),
        command = 'runInTerminal',
        arguments = arguments
    }
    return true
end

function m.send(pkg)
    if server then
        if pkg.type == 'response' and pkg.command == 'runInTerminal' then
            return
        end
        server.send(pkg)
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
            if pkg.command == 'attach' then
                local args = pkg.arguments
                if args.processId or args.processName then
                    response_error(pkg, 'not support')
                    return
                end
                local ip = args.ip
                local port = args.port
                if ip == 'localhost' then
                    ip = '127.0.0.1'
                end
                server = serverFactory.tcp_client(m, ip, port)
                server.send(initReq)
                server.send(pkg)
            elseif pkg.command == 'launch' then
                local args = pkg.arguments
                if args.console == 'integratedTerminal' or args.console == 'externalTerminal' then
                    local port
                    server, port = serverFactory.unix_server()
                    if not create_terminal(args, port) then
                        response_error(pkg, 'launch failed')
                        return
                    end
                    server.send(initReq)
                    server.send(pkg)
                    return
                end
                response_error(pkg, 'not support')
            else
                response_error(pkg, 'error request')
            end
        end
    end
end

function m.recv(pkg)
    client.send(pkg)
end

function m.update()
    if server then
        while true do
            local pkg = server.recv()
            if pkg then
                io.send(pkg)
            else
                break
            end
        end
    end
end

function m.init(io_)
    io = io_
end

return m
