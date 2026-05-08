-- shared disassembly logic, version dispatch
-- version is always 53/54/55 (luaver.LUAVERSION format)

local modules = {}

-- Lua 5.3: lineinfo stores direct line numbers
local function get_source_line_53(func, pc)
    if not func.lineinfo or #func.lineinfo == 0 then return nil end
    local idx = pc + 1
    if idx <= #func.lineinfo then return func.lineinfo[idx] end
end

-- Lua 5.4+: lineinfo stores signed byte deltas with abslineinfo anchors
local function get_source_line_54plus(func, pc)
    if not func.lineinfo or #func.lineinfo == 0 then return nil end
    local basepc = -1
    local baseline = func.linedefined or 0
    if func.abslineinfo and #func.abslineinfo > 0 then
        if pc < func.abslineinfo[1].pc then
            basepc = -1
            baseline = func.linedefined
        else
            local i = math.floor(pc / 128) - 1
            if i < 1 then i = 1 end
            while i + 1 <= #func.abslineinfo and pc >= func.abslineinfo[i + 1].pc do
                i = i + 1
            end
            basepc = func.abslineinfo[i].pc
            baseline = func.abslineinfo[i].line
        end
    end
    local line = baseline
    local cur = basepc
    while cur < pc do
        cur = cur + 1
        if cur > #func.lineinfo then break end
        local li = func.lineinfo[cur + 1]
        if li == -128 and func.abslineinfo then
            for j = 1, #func.abslineinfo do
                if func.abslineinfo[j].pc > cur then
                    cur = func.abslineinfo[j].pc
                    line = func.abslineinfo[j].line
                    break
                end
            end
        else
            line = line + li
        end
    end
    return line
end

local function get_module(version)
    local v = version
    if not modules[v] then
        if v == 53 then
            modules[v] = require "backend.worker.disassemble.lua53"
        elseif v == 54 then
            modules[v] = require "backend.worker.disassemble.lua54"
        elseif v == 55 then
            modules[v] = require "backend.worker.disassemble.lua55"
        end
    end
    return modules[v]
end

local function disassemble_function(func, version)
    local mod = get_module(version)
    if not mod then
        return {}
    end
    local instructions = {}
    if not func.code or #func.code == 0 then
        return instructions
    end

    local get_line = version == 53 and get_source_line_53 or get_source_line_54plus
    local extra_arg = mod.precompute(func)

    for i = 1, #func.code do
        local inst = func.code[i]
        local d = mod.decode(inst)
        local pc = i - 1
        local opName = mod.OPCODES[d.op + 1] or ("UNKNOWN_%d"):format(d.op)
        local operands = mod.format(d)
        local cmt = mod.comment(opName, d, func, pc, extra_arg[pc])
        local line = get_line(func, pc)
        local hint = nil
        if opName == "EXTRAARG" or opName == "VARARGPREP" then
            hint = "invalid"
        end
        local r = {
            pc = pc,
            opName = opName,
            operands = operands,
            comment = cmt,
            line = line,
            address = ("0x%08X"):format(pc),
            instructionBytes = ("%08X"):format(inst & 0xFFFFFFFF),
        }
        if hint then
            r.presentationHint = hint
        end
        if mod.symbol then
            local s = mod.symbol(func, pc, d)
            if s then r.symbol = s end
        end
        instructions[#instructions + 1] = r
    end
    return instructions
end

return {
    disassemble_function = disassemble_function,
    get_source_line = get_source_line,
}
