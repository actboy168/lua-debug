-- Lua 5.4 opcodes and disassembly

-- opcode names (order from lopcodes.h ORDER OP)
local OPCODES = {
    "MOVE", "LOADI", "LOADF", "LOADK", "LOADKX",
    "LOADFALSE", "LFALSESKIP", "LOADTRUE", "LOADNIL",
    "GETUPVAL", "SETUPVAL", "GETTABUP", "GETTABLE", "GETI", "GETFIELD",
    "SETTABUP", "SETTABLE", "SETI", "SETFIELD", "NEWTABLE",
    "SELF", "ADDI", "ADDK", "SUBK", "MULK", "MODK", "POWK", "DIVK", "IDIVK",
    "BANDK", "BORK", "BXORK", "SHRI", "SHLI",
    "ADD", "SUB", "MUL", "MOD", "POW", "DIV", "IDIV",
    "BAND", "BOR", "BXOR", "SHL", "SHR",
    "MMBIN", "MMBINI", "MMBINK",
    "UNM", "BNOT", "NOT", "LEN",
    "CONCAT", "CLOSE", "TBC", "JMP",
    "EQ", "LT", "LE", "EQK", "EQI", "LTI", "LEI", "GTI", "GEI",
    "TEST", "TESTSET",
    "CALL", "TAILCALL", "RETURN", "RETURN0", "RETURN1",
    "FORLOOP", "FORPREP", "TFORPREP", "TFORCALL", "TFORLOOP",
    "SETLIST", "CLOSURE",
    "VARARG", "VARARGPREP", "EXTRAARG",
}

-- opmode flags from lopcodes.c: opmode(mm, ot, it, t, a, mode)
-- layout: {mm, ot, it, t, a, mode}
local OPMODES = {
    {0, 0, 0, 0, 1, "iABC"},   -- MOVE
    {0, 0, 0, 0, 1, "iAsBx"},  -- LOADI
    {0, 0, 0, 0, 1, "iAsBx"},  -- LOADF
    {0, 0, 0, 0, 1, "iABx"},   -- LOADK
    {0, 0, 0, 0, 1, "iABx"},   -- LOADKX
    {0, 0, 0, 0, 1, "iABC"},   -- LOADFALSE
    {0, 0, 0, 0, 1, "iABC"},   -- LFALSESKIP
    {0, 0, 0, 0, 1, "iABC"},   -- LOADTRUE
    {0, 0, 0, 0, 1, "iABC"},   -- LOADNIL
    {0, 0, 0, 0, 1, "iABC"},   -- GETUPVAL
    {0, 0, 0, 0, 0, "iABC"},   -- SETUPVAL
    {0, 0, 0, 0, 1, "iABC"},   -- GETTABUP
    {0, 0, 0, 0, 1, "iABC"},   -- GETTABLE
    {0, 0, 0, 0, 1, "iABC"},   -- GETI
    {0, 0, 0, 0, 1, "iABC"},   -- GETFIELD
    {0, 0, 0, 0, 0, "iABC"},   -- SETTABUP
    {0, 0, 0, 0, 0, "iABC"},   -- SETTABLE
    {0, 0, 0, 0, 0, "iABC"},   -- SETI
    {0, 0, 0, 0, 0, "iABC"},   -- SETFIELD
    {0, 0, 0, 0, 1, "iABC"},   -- NEWTABLE
    {0, 0, 0, 0, 1, "iABC"},   -- SELF
    {0, 0, 0, 0, 1, "iABC"},   -- ADDI
    {0, 0, 0, 0, 1, "iABC"},   -- ADDK
    {0, 0, 0, 0, 1, "iABC"},   -- SUBK
    {0, 0, 0, 0, 1, "iABC"},   -- MULK
    {0, 0, 0, 0, 1, "iABC"},   -- MODK
    {0, 0, 0, 0, 1, "iABC"},   -- POWK
    {0, 0, 0, 0, 1, "iABC"},   -- DIVK
    {0, 0, 0, 0, 1, "iABC"},   -- IDIVK
    {0, 0, 0, 0, 1, "iABC"},   -- BANDK
    {0, 0, 0, 0, 1, "iABC"},   -- BORK
    {0, 0, 0, 0, 1, "iABC"},   -- BXORK
    {0, 0, 0, 0, 1, "iABC"},   -- SHRI
    {0, 0, 0, 0, 1, "iABC"},   -- SHLI
    {0, 0, 0, 0, 1, "iABC"},   -- ADD
    {0, 0, 0, 0, 1, "iABC"},   -- SUB
    {0, 0, 0, 0, 1, "iABC"},   -- MUL
    {0, 0, 0, 0, 1, "iABC"},   -- MOD
    {0, 0, 0, 0, 1, "iABC"},   -- POW
    {0, 0, 0, 0, 1, "iABC"},   -- DIV
    {0, 0, 0, 0, 1, "iABC"},   -- IDIV
    {0, 0, 0, 0, 1, "iABC"},   -- BAND
    {0, 0, 0, 0, 1, "iABC"},   -- BOR
    {0, 0, 0, 0, 1, "iABC"},   -- BXOR
    {0, 0, 0, 0, 1, "iABC"},   -- SHL
    {0, 0, 0, 0, 1, "iABC"},   -- SHR
    {1, 0, 0, 0, 0, "iABC"},   -- MMBIN
    {1, 0, 0, 0, 0, "iABC"},   -- MMBINI
    {1, 0, 0, 0, 0, "iABC"},   -- MMBINK
    {0, 0, 0, 0, 1, "iABC"},   -- UNM
    {0, 0, 0, 0, 1, "iABC"},   -- BNOT
    {0, 0, 0, 0, 1, "iABC"},   -- NOT
    {0, 0, 0, 0, 1, "iABC"},   -- LEN
    {0, 0, 0, 0, 1, "iABC"},   -- CONCAT
    {0, 0, 0, 0, 0, "iABC"},   -- CLOSE
    {0, 0, 0, 0, 0, "iABC"},   -- TBC
    {0, 0, 0, 0, 0, "isJ"},    -- JMP
    {0, 0, 0, 1, 0, "iABC"},   -- EQ
    {0, 0, 0, 1, 0, "iABC"},   -- LT
    {0, 0, 0, 1, 0, "iABC"},   -- LE
    {0, 0, 0, 1, 0, "iABC"},   -- EQK
    {0, 0, 0, 1, 0, "iABC"},   -- EQI
    {0, 0, 0, 1, 0, "iABC"},   -- LTI
    {0, 0, 0, 1, 0, "iABC"},   -- LEI
    {0, 0, 0, 1, 0, "iABC"},   -- GTI
    {0, 0, 0, 1, 0, "iABC"},   -- GEI
    {0, 0, 0, 1, 0, "iABC"},   -- TEST
    {0, 0, 0, 1, 1, "iABC"},   -- TESTSET
    {0, 1, 1, 0, 1, "iABC"},   -- CALL
    {0, 1, 1, 0, 1, "iABC"},   -- TAILCALL
    {0, 0, 1, 0, 0, "iABC"},   -- RETURN
    {0, 0, 0, 0, 0, "iABC"},   -- RETURN0
    {0, 0, 0, 0, 0, "iABC"},   -- RETURN1
    {0, 0, 0, 0, 1, "iABx"},   -- FORLOOP
    {0, 0, 0, 0, 1, "iABx"},   -- FORPREP
    {0, 0, 0, 0, 0, "iABx"},   -- TFORPREP
    {0, 0, 0, 0, 0, "iABC"},   -- TFORCALL
    {0, 0, 0, 0, 1, "iABx"},   -- TFORLOOP
    {0, 0, 1, 0, 0, "iABC"},   -- SETLIST
    {0, 0, 0, 0, 1, "iABx"},   -- CLOSURE
    {0, 1, 0, 0, 1, "iABC"},   -- VARARG
    {0, 0, 1, 0, 1, "iABC"},   -- VARARGPREP
    {0, 0, 0, 0, 0, "iAx"},    -- EXTRAARG
}

-- offsets from lopcodes.h
local OFFSET_sC = 0x7F     -- 127
local OFFSET_sBx = 0xFFFF  -- 65535
local OFFSET_sJ = 0xFFFFFF -- 16777215

local function decode(inst)
    inst = inst & 0xFFFFFFFF
    local op = inst & 0x7F
    local A  = (inst >> 7) & 0xFF
    local k  = (inst >> 15) & 1
    local B  = (inst >> 16) & 0xFF
    local C  = (inst >> 24) & 0xFF
    local Bx = (inst >> 15) & 0x1FFFF
    local sBx = Bx - OFFSET_sBx
    local Ax = (inst >> 7) & 0x1FFFFFF
    local sJ = Ax - OFFSET_sJ
    local sB = B - OFFSET_sC
    local sC = C - OFFSET_sC
    return {
        op = op, A = A, k = k, B = B, C = C,
        Bx = Bx, sBx = sBx, Ax = Ax, sJ = sJ,
        sB = sB, sC = sC, vB = 0, vC = 0,
    }
end

-- metamethod names from lopcodes.h TM enum
local META_METHODS = {
    "__index", "__newindex", "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow", "__div", "__idiv",
    "__band", "__bor", "__bxor", "__shl", "__shr",
    "__unm", "__bnot", "__lt", "__le", "__concat", "__call", "__close",
}

local function format(d)
    local opName = OPCODES[d.op + 1]
    if not opName then return "?" end
    if opName == "MOVE" or opName == "GETUPVAL" or opName == "SETUPVAL"
        or opName == "UNM" or opName == "BNOT" or opName == "NOT" or opName == "LEN"
        or opName == "CLOSE" or opName == "TBC" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "LOADI" or opName == "LOADF" then
        return ("%d %d"):format(d.A, d.sBx)
    elseif opName == "LOADK" or opName == "CLOSURE" then
        return ("%d %d"):format(d.A, d.Bx)
    elseif opName == "LOADKX" then
        return ("%d"):format(d.A)
    elseif opName == "LOADFALSE" or opName == "LFALSESKIP" or opName == "LOADTRUE"
        or opName == "VARARGPREP" then
        return ("%d"):format(d.A)
    elseif opName == "LOADNIL" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "GETTABUP" or opName == "SETTABUP" or opName == "GETTABLE"
        or opName == "SETTABLE" or opName == "GETI" or opName == "SETI"
        or opName == "GETFIELD" or opName == "SETFIELD" or opName == "SELF"
        or opName == "NEWTABLE" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "ADD" or opName == "SUB" or opName == "MUL" or opName == "MOD"
        or opName == "POW" or opName == "DIV" or opName == "IDIV"
        or opName == "BAND" or opName == "BOR" or opName == "BXOR"
        or opName == "SHL" or opName == "SHR" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "ADDI" then
        return ("%d %d %d"):format(d.A, d.B, d.sC)
    elseif opName == "ADDK" or opName == "SUBK" or opName == "MULK" or opName == "MODK"
        or opName == "POWK" or opName == "DIVK" or opName == "IDIVK" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "BANDK" or opName == "BORK" or opName == "BXORK" then
        return ("%d %d"):format(d.A, d.C)
    elseif opName == "SHRI" or opName == "SHLI" then
        return ("%d %d %d"):format(d.A, d.B, d.sC)
    elseif opName == "EQ" or opName == "LT" or opName == "LE" then
        return ("%d %d %d %d"):format(d.A, d.B, d.C, d.k)
    elseif opName == "EQK" then
        return ("%d %d %d"):format(d.A, d.B, d.k)
    elseif opName == "EQI" or opName == "LTI" or opName == "LEI"
        or opName == "GTI" or opName == "GEI" then
        return ("%d %d"):format(d.A, d.sB)
    elseif opName == "TEST" then
        return ("%d %d"):format(d.A, d.k)
    elseif opName == "TESTSET" then
        return ("%d %d %d"):format(d.A, d.B, d.k)
    elseif opName == "MMBIN" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "MMBINI" then
        return ("%d %d %d %d"):format(d.A, d.sB, d.C, d.k)
    elseif opName == "MMBINK" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "CALL" or opName == "TAILCALL" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "RETURN" then
        return ("%d %d"):format(d.A, d.B)
    elseif opName == "RETURN0" then
        return ""
    elseif opName == "RETURN1" then
        return ("%d"):format(d.A)
    elseif opName == "JMP" then
        return ("%d"):format(d.sJ)
    elseif opName == "FORLOOP" or opName == "FORPREP" or opName == "TFORPREP"
        or opName == "TFORLOOP" then
        return ("%d %d"):format(d.A, d.Bx)
    elseif opName == "TFORCALL" then
        return ("%d %d"):format(d.A, d.C)
    elseif opName == "SETLIST" then
        return ("%d %d %d"):format(d.A, d.B, d.C)
    elseif opName == "VARARG" then
        return ("%d %d"):format(d.A, d.C)
    elseif opName == "CONCAT" then
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

    if opName == "GETTABUP" or opName == "SETTABUP" then
        local uv = upvalues[d.B + 1]
        local uvname = uv and uv.name or ""
        local cname = constants[d.C + 1] and ("%q"):format(constants[d.C + 1]) or ""
        return ("%s %s"):format(uvname, cname)
    elseif opName == "LOADK" then
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
    elseif opName == "MMBIN" then
        return META_METHODS[d.C + 1] or ""
    elseif opName == "MMBINI" or opName == "MMBINK" then
        local s = META_METHODS[d.C + 1] or ""
        if d.k ~= 0 then s = s .. " flip" end
        return s
    elseif opName == "JMP" then
        return ("to %d"):format(pc + d.sJ)
    elseif opName == "FORLOOP" then
        return ("to %d"):format(pc - d.Bx)
    elseif opName == "FORPREP" then
        return ("exit to %d"):format(pc + d.Bx + 1)
    elseif opName == "TFORPREP" then
        return ("to %d"):format(pc + d.Bx)
    elseif opName == "TFORLOOP" then
        return ("to %d"):format(pc - d.Bx)
    end
    return ""
end

-- try to resolve a function name for CALL/TAILCALL by looking backwards
-- for the instruction that loaded the function into the target register
local function symbol(func, pc, d)
    if pc == 0 then return nil end
    local constants = func.k or {}
    local upvalues = func.upvalues or {}

    if d.op ~= 68 and d.op ~= 69 then return nil end -- CALL / TAILCALL

    -- walk backwards up to 4 instructions to find what wrote to R[A]
    for step = 1, math.min(4, pc) do
        local prev = func.code[pc - step + 1]  -- func.code is 1-indexed
        if not prev then break end
        prev = prev & 0xFFFFFFFF
        local prevOp = prev & 0x7F
        local prevA  = (prev >> 7) & 0xFF
        if prevA == d.A then
            if prevOp == 11 then -- GETTABUP
                local uv = upvalues[(prev >> 16) & 0xFF + 1]
                local cidx = (prev >> 24) & 0xFF
                local uvname = uv and uv.name or ("_UPV[%d]"):format((prev >> 16) & 0xFF)
                local cname = constants[cidx + 1] and tostring(constants[cidx + 1]) or "?"
                return ("%s.%s"):format(uvname, cname)
            elseif prevOp == 13 then -- GETFIELD
                local cidx = (prev >> 24) & 0xFF
                if constants[cidx + 1] then return tostring(constants[cidx + 1]) end
            elseif prevOp == 4 then -- LOADK
                local bidx = (prev >> 15) & 0x1FFFF
                if constants[bidx + 1] then return tostring(constants[bidx + 1]) end
            elseif prevOp == 20 then -- SELF
                local cidx = (prev >> 24) & 0xFF
                if constants[cidx + 1] then return tostring(constants[cidx + 1]) end
            elseif prevOp == 79 then -- CLOSURE
                return ("closure_%d"):format((prev >> 15) & 0x1FFFF)
            end
            break -- found matching register but unrecognized op
        end
    end
    return nil
end

local function precompute(func)
    local extra = {}
    for i = 1, #func.code do
        local ci = func.code[i] & 0xFFFFFFFF
        local op = ci & 0x7F
        if op == 4 then -- LOADKX
            if i + 1 <= #func.code then
                local ni = func.code[i + 1] & 0xFFFFFFFF
                if (ni & 0x7F) == 82 then -- EXTRAARG
                    extra[i] = { Ax = (ni >> 7) & 0x1FFFFFF }
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
