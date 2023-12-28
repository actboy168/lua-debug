local undump = require 'backend.worker.undump'

local version

local function getproto(content)
    local f = load(content)
    if not f then
        return
    end
    local bin = string.dump(f)
    local cl, v = undump(bin)
    version = v
    return cl.f
end

local function nextline(proto, abs, currentline, pc)
    local line = proto.lineinfo[pc]
    if line == -128 then
        return assert(abs[pc-1])
    else
        return currentline + line
    end
end

local function getactivelines(proto)
    local l = {}
    if version >= 0x54 then
        local currentline = proto.linedefined
        local abs = {}
        for _, line in ipairs(proto.abslineinfo) do
            abs[line.pc] = line.line
        end
        local start = 1
        if proto.is_vararg > 0 then
            assert(proto.code[1] & 0x7F == 81) -- OP_VARARGPREP
            currentline = nextline(proto, abs, currentline, 1)
            start = 2
        end
        for pc = start, #proto.lineinfo do
            currentline = nextline(proto, abs, currentline, pc)
            l[currentline] = true
        end
    else
        for _, line in ipairs(proto.lineinfo) do
            l[line] = true
        end
    end
    return l
end

local function calclineinfo(proto, lineinfo, si)
    local activelines = getactivelines(proto)
    local startLn = proto.linedefined
    local endLn = proto.lastlinedefined
    local key = startLn.."-"..endLn
    if endLn == 0 then
        startLn = 1
        for l in pairs(activelines) do
            endLn = math.max(endLn, l)
        end
    end
    for l in pairs(activelines) do
        si.activelines[l] = true
    end
    for l = startLn, endLn do
        si.definelines[l] = key
    end
    lineinfo[key] = activelines
    for i = 1, proto.sizep do
        calclineinfo(proto.p[i], lineinfo, si)
    end
end

local function nextActiveLine(si, line)
    local defines = si.definelines
    local actives = si.activelines
    local fn = defines[line]
    while actives[line] ~= true do
        if fn ~= defines[line] then
            return
        end
        line = line + 1
    end
    return line
end

local function normalize(lineinfo, si)
    local maxline = 0
    for l in pairs(si.definelines) do
        maxline = math.max(maxline, l)
    end
    for i = 1, maxline do
        lineinfo[i] = nextActiveLine(si, i)
    end
end

return function (content)
    local proto = getproto(content)
    if not proto then
        return
    end
    local si = { activelines = {}, definelines = {} }
    local lineinfo = {}
    calclineinfo(proto, lineinfo, si)
    normalize(lineinfo, si)
    return lineinfo
end
