-- Lua 5.3 opcodes and disassembly
-- 6-bit opcode, 8-bit A, 9-bit B/C, RK operands

-- opcode names (order from lopcodes.c ORDER OP, 47 total)
local OPCODES = {
    "MOVE", "LOADK", "LOADKX", "LOADBOOL", "LOADNIL",
    "GETUPVAL", "GETTABUP", "GETTABLE",
    "SETTABUP", "SETUPVAL", "SETTABLE",
    "NEWTABLE", "SELF",
    "ADD", "SUB", "MUL", "MOD", "POW", "DIV", "IDIV",
    "BAND", "BOR", "BXOR", "SHL", "SHR",
    "UNM", "BNOT", "NOT", "LEN",
    "CONCAT", "JMP",
    "EQ", "LT", "LE", "TEST", "TESTSET",
    "CALL", "TAILCALL", "RETURN",
    "FORLOOP", "FORPREP", "TFORCALL", "TFORLOOP",
    "SETLIST", "CLOSURE", "VARARG", "EXTRAARG",
}

-- opmode flags from lopcodes.c: opmode(t, a, b, c, mode)
-- layout: {t, a, b, c, mode}
local OPMODES = {
    {0, 1, 0, 0, "iABC"},  -- MOVE (OpArgR, OpArgN)
    {0, 1, 1, 0, "iABx"},  -- LOADK (OpArgK, OpArgN)
    {0, 1, 0, 0, "iABx"},  -- LOADKX
    {0, 1, 2, 2, "iABC"},  -- LOADBOOL (OpArgU, OpArgU)
    {0, 1, 2, 0, "iABC"},  -- LOADNIL (OpArgU, OpArgN)
    {0, 1, 2, 0, "iABC"},  -- GETUPVAL
    {0, 1, 2, 1, "iABC"},  -- GETTABUP (OpArgU, OpArgK)
    {0, 1, 0, 1, "iABC"},  -- GETTABLE (OpArgR, OpArgK)
    {0, 0, 1, 1, "iABC"},  -- SETTABUP (OpArgK, OpArgK)
    {0, 0, 2, 0, "iABC"},  -- SETUPVAL
    {0, 0, 1, 1, "iABC"},  -- SETTABLE
    {0, 1, 2, 2, "iABC"},  -- NEWTABLE
    {0, 1, 0, 1, "iABC"},  -- SELF
    {0, 1, 1, 1, "iABC"},  -- ADD
    {0, 1, 1, 1, "iABC"},  -- SUB
    {0, 1, 1, 1, "iABC"},  -- MUL
    {0, 1, 1, 1, "iABC"},  -- MOD
    {0, 1, 1, 1, "iABC"},  -- POW
    {0, 1, 1, 1, "iABC"},  -- DIV
    {0, 1, 1, 1, "iABC"},  -- IDIV
    {0, 1, 1, 1, "iABC"},  -- BAND
    {0, 1, 1, 1, "iABC"},  -- BOR
    {0, 1, 1, 1, "iABC"},  -- BXOR
    {0, 1, 1, 1, "iABC"},  -- SHL
    {0, 1, 1, 1, "iABC"},  -- SHR
    {0, 1, 0, 0, "iABC"},  -- UNM
    {0, 1, 0, 0, "iABC"},  -- BNOT
    {0, 1, 0, 0, "iABC"},  -- NOT
    {0, 1, 0, 0, "iABC"},  -- LEN
    {0, 1, 0, 0, "iABC"},  -- CONCAT
    {0, 0, 0, 0, "iAsBx"}, -- JMP
    {1, 0, 1, 1, "iABC"},  -- EQ
    {1, 0, 1, 1, "iABC"},  -- LT
    {1, 0, 1, 1, "iABC"},  -- LE
    {1, 0, 0, 2, "iABC"},  -- TEST
    {1, 1, 0, 2, "iABC"},  -- TESTSET
    {0, 1, 2, 2, "iABC"},  -- CALL
    {0, 1, 2, 2, "iABC"},  -- TAILCALL
    {0, 0, 2, 0, "iABC"},  -- RETURN
    {0, 1, 0, 0, "iAsBx"}, -- FORLOOP
    {0, 1, 0, 0, "iAsBx"}, -- FORPREP
    {0, 0, 0, 2, "iABC"},  -- TFORCALL
    {0, 1, 0, 0, "iAsBx"}, -- TFORLOOP
    {0, 0, 2, 2, "iABC"},  -- SETLIST
    {0, 1, 2, 0, "iABx"},  -- CLOSURE
    {0, 1, 2, 0, "iABC"},  -- VARARG
    {0, 0, 2, 2, "iAx"},   -- EXTRAARG
}

-- offsets from lopcodes.h
local MAXARG_sBx = 131071
local OFFSET_sBx = MAXARG_sBx >> 1  -- 65535
local BITRK = 256  -- 1 << (SIZE_B - 1)

local function decode(inst)
    inst = inst & 0xFFFFFFFF
    local op = inst & 0x3F         -- 6-bit opcode
    local A  = (inst >> 6) & 0xFF  -- 8-bit A at pos 6
    local C  = (inst >> 14) & 0x1FF -- 9-bit C at pos 14
    local B  = (inst >> 23) & 0x1FF -- 9-bit B at pos 23
    local Bx = (inst >> 14) & 0x3FFFF -- C+B together (18-bit)
    local sBx = Bx - OFFSET_sBx
    local Ax = (inst >> 6) & 0x3FFFFFF -- 26-bit Ax
    return {
        op = op, A = A, B = B, C = C,
        Bx = Bx, sBx = sBx, Ax = Ax,
    }
end

-- metamethod names (same as 5.4)
local META_METHODS = {
    "__index", "__newindex", "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow", "__div", "__idiv",
    "__band", "__bor", "__bxor", "__shl", "__shr",
    "__unm", "__bnot", "__lt", "__le", "__concat", "__call",
}

-- RK format: if arg >= 256, it's a constant index (arg - 256)
-- otherwise it's a register index
local function RK(arg, isK)
    if arg >= BITRK then
        return ("K[%d]"):format(arg - BITRK)
    else
        return ("R[%d]"):format(arg)
    end
end

local function format(d)
    local opName = OPCODES[d.op + 1]
    if not opName then return "?" end

    if opName == "MOVE" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "LOADK" then
        return ("%d %d"):format(d.A, d.Bx)
    elseif opName == "LOADKX" then
        return ("%d"):format(d.A)
    elseif opName == "LOADBOOL" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "LOADNIL" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "GETUPVAL" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "GETTABUP" or opName == "GETTABLE" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "SETTABUP" or opName == "SETTABLE" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "SETUPVAL" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "NEWTABLE" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "SELF" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "ADD" or opName == "SUB" or opName == "MUL" or opName == "MOD"
        or opName == "POW" or opName == "DIV" or opName == "IDIV"
        or opName == "BAND" or opName == "BOR" or opName == "BXOR"
        or opName == "SHL" or opName == "SHR" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "UNM" or opName == "BNOT" or opName == "NOT" or opName == "LEN" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "CONCAT" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "JMP" then
        return ("%d %d"):format(d.A, d.sBx)
    elseif opName == "EQ" or opName == "LT" or opName == "LE" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "TEST" then
        return ("%d %d"):format(d.A, d.C)
    elseif opName == "TESTSET" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "CALL" or opName == "TAILCALL" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "RETURN" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "FORLOOP" or opName == "FORPREP" or opName == "TFORLOOP" then
        return ("%d %d"):format(d.A, d.sBx)
    elseif opName == "TFORCALL" then
        return ("%d %d"):format(d.A, d.C)
    elseif opName == "SETLIST" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "CLOSURE" then
        return ("%d %d"):format(d.A, d.Bx)
    elseif opName == "VARARG" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "EXTRAARG" then
        return ("%d"):format(d.Ax)
    else
        return ("%d %d %d"):format(d.A, d.B, d.C)
    end
end

local function comment(opName, d, func, pc, extra)
    local constants = func.k or {}
    local upvalues = func.upvalues or {}
    local protos = func.p or {}

    if opName == "LOADK" then
        if constants[d.Bx + 1] ~= nil then
            local c = constants[d.Bx + 1]
            if type(c) == "string" then return ("%q"):format(c)
            elseif c == nil then return "nil"
            elseif type(c) == "boolean" then return c and "true" or "false"
            else return tostring(c) end
        end
    elseif opName == "LOADKX" then
        if extra then
            local ax = extra.Ax
            if constants[ax + 1] ~= nil then
                local c = constants[ax + 1]
                if type(c) == "string" then return ("%q"):format(c)
                elseif c == nil then return "nil"
                elseif type(c) == "boolean" then return c and "true" or "false"
                else return tostring(c) end
            end
        end
    elseif opName == "EXTRAARG" then
        return ("Ax=%d"):format(d.Ax)
    elseif opName == "CLOSURE" then
        if protos[d.Bx + 1] then
            local proto = protos[d.Bx + 1]
            local params = {}
            if proto.locvars then
                for i = 1, proto.numparams do
                    local lv = proto.locvars[i]
                    params[#params + 1] = (lv and lv.varname) or ("param_%d"):format(i)
                end
            end
            if proto.is_vararg and proto.is_vararg ~= 0 then
                params[#params + 1] = "..."
            end
            return ("function %s(%s)"):format(("function_%d"):format(d.Bx), table.concat(params, ", "))
        end
    elseif opName == "CALL" or opName == "TAILCALL" then
        local nargs = d.B > 0 and (d.B - 1) or 0
        local nresults = d.C > 0 and (d.C - 1) or 0
        return ("%d in %d out"):format(nargs, nresults)
    elseif opName == "RETURN" then
        local nresults = d.B > 0 and (d.B - 1) or 0
        return ("%d out"):format(nresults)
    elseif opName == "JMP" then
        return ("to %d"):format(pc + d.sBx)
    elseif opName == "FORLOOP" or opName == "TFORLOOP" then
        return ("to %d"):format(pc + d.sBx)
    elseif opName == "FORPREP" then
        return ("exit to %d"):format(pc + d.sBx)
    end
    return ""
end

local function symbol(func, pc, d)
    if pc == 0 then return nil end
    local constants = func.k or {}
    local upvalues = func.upvalues or {}
    if d.op ~= 36 and d.op ~= 37 then return nil end  -- CALL / TAILCALL
    for step = 1, math.min(4, pc) do
        local prev = func.code[pc - step + 1]
        if not prev then break end
        prev = prev & 0xFFFFFFFF
        local prevOp = prev & 0x3F
        local prevA  = (prev >> 6) & 0xFF
        if prevA == d.A then
            if prevOp == 6 then -- GETTABUP
                local uv = upvalues[(prev >> 23) & 0x1FF + 1]
                local cidx = (prev >> 14) & 0x1FF
                local uvname = uv and uv.name or ("_UPV[%d]"):format((prev >> 23) & 0x1FF)
                local cname = cidx >= 256 and constants[cidx - 256 + 1] and tostring(constants[cidx - 256 + 1]) or "?"
                return ("%s.%s"):format(uvname, cname)
            elseif prevOp == 1 then -- LOADK
                local bidx = (prev >> 14) & 0x3FFFF
                if constants[bidx + 1] then return tostring(constants[bidx + 1]) end
            elseif prevOp == 12 then -- SELF
                local cidx = (prev >> 14) & 0x1FF
                if cidx >= 256 and constants[cidx - 256 + 1] then
                    return tostring(constants[cidx - 256 + 1])
                end
            elseif prevOp == 44 then -- CLOSURE
                return ("closure_%d"):format((prev >> 14) & 0x3FFFF)
            end
            break
        end
    end
    return nil
end

local function precompute(func)
    local extra = {}
    for i = 1, #func.code do
        local ci = func.code[i] & 0xFFFFFFFF
        local op = ci & 0x3F
        if op == 2 then -- LOADKX
            if i + 1 <= #func.code then
                local ni = func.code[i + 1] & 0xFFFFFFFF
                if (ni & 0x3F) == 46 then -- EXTRAARG
                    extra[i] = { Ax = (ni >> 6) & 0x3FFFFFF }
                end
            end
        end
    end
    return extra
end

return {
    OPCODES = OPCODES,
    OPMODES = OPMODES,
    decode = decode,
    format = format,
    comment = comment,
    precompute = precompute,
    symbol = symbol,
}
