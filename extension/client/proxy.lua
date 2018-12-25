local serverFactory = require 'serverFactory'
local debuggerFactory = require 'debugerFactory'
local fs = require 'bee.filesystem'
local inject = require 'inject'
local server
local client
local seq = 0
local initReq
local m = {}

local function getVersion(dbg)
    local json = require 'json'
    local package = json.decode(assert(io.open((dbg / 'package.json'):string()):read 'a'))
    return package.version
end

local function getDbgPath()
    if WORKDIR:filename():string() ~= 'extension' then
        return WORKDIR
    end
    return fs.path(os.getenv 'USERPROFILE') / '.vscode' / 'extensions' / ('actboy168.lua-debug-' .. getVersion(dbg))
end

local function getUnixPath(pid)
    local path = getDbgPath() / "windows" / "tmp"
    fs.create_directories(path)
	return path / ("pid_%d.tmp"):format(pid)
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
    local arguments = debuggerFactory.create_terminal(args, getDbgPath(), port)
    if not arguments then
        return
    end
    request_runinterminal(arguments)
    return true
end

local function proxy_attach(pkg)
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
end

local function proxy_launch(pkg)
    local args = pkg.arguments
    if args.console == 'integratedTerminal' or args.console == 'externalTerminal' then
        local path = getUnixPath(inject.current_pid())
        fs.remove(path)
        server = serverFactory {
            protocol = 'unix',
            address = path:string()
        }
        if not create_terminal(args, path) then
            response_error(pkg, 'launch failed')
            return
        end
        server.send(initReq)
        server.send(pkg)
        return
    end
    response_error(pkg, 'not support')
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
                proxy_attach(pkg)
            elseif pkg.command == 'launch' then
                proxy_launch(pkg)
            else
                response_error(pkg, 'error request')
            end
        end
    end
end

function m.update()
    if server then
        while true do
            local pkg = server.recv()
            if pkg then
                client.send(pkg)
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
