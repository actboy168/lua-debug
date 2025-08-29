local unpack_buf = ""
local unpack_pos = 1
local function unpack_setpos(...)
    unpack_pos = select(-1, ...)
    return ...
end
local function unpack(fmt)
    return unpack_setpos(fmt:unpack(unpack_buf, unpack_pos))
end

local function LoadByte()
    return unpack "B"
end

local function LoadInteger()
    return unpack "j"
end

local function LoadNumber()
    return unpack "n"
end

local function LoadCharN(n)
    return unpack("c"..tostring(n))
end

local undump53; do
    local function LoadInt()
        return unpack "i"
    end

    local function LoadString()
        local size = LoadByte()
        if size == 0xFF then
            size = unpack "T"
        end
        if size == 0 then
            return nil
        end
        return LoadCharN(size - 1)
    end

    local LoadFunction

    local function LoadCode(f)
        f.sizecode = LoadInt()
        f.code = {}
        for i = 1, f.sizecode do
            f.code[i] = unpack "i4"
        end
    end

    local function LoadConstants(f)
        local LUA_TNIL = 0
        local LUA_TBOOLEAN = 1
        local LUA_TNUMFLT = 3 | (0 << 4)
        local LUA_TNUMINT = 3 | (1 << 4)
        local LUA_TSHRSTR = 4 | (0 << 4)
        local LUA_TLNGSTR = 4 | (1 << 4)
        f.sizek = LoadInt()
        f.k = {}
        for i = 1, f.sizek do
            local t = LoadByte()
            if t == LUA_TNIL then
            elseif t == LUA_TBOOLEAN then
                f.k[i] = LoadByte()
            elseif t == LUA_TNUMFLT then
                f.k[i] = LoadNumber()
            elseif t == LUA_TNUMINT then
                f.k[i] = LoadInteger()
            elseif t == LUA_TSHRSTR then
                f.k[i] = LoadString()
            elseif t == LUA_TLNGSTR then
                f.k[i] = LoadString()
            else
                error("unknown type: "..t)
            end
        end
    end

    local function LoadUpvalues(f)
        f.sizeupvalues = LoadInt()
        f.upvalues = {}
        for i = 1, f.sizeupvalues do
            f.upvalues[i] = {}
            f.upvalues[i].instack = LoadByte()
            f.upvalues[i].idx = LoadByte()
        end
    end

    local function LoadProtos(f)
        f.sizep = LoadInt()
        f.p = {}
        for i = 1, f.sizep do
            f.p[i] = {}
            LoadFunction(f.p[i], f.source)
        end
    end

    local function LoadDebug(f)
        f.sizelineinfo = LoadInt()
        f.lineinfo = {}
        for i = 1, f.sizelineinfo do
            f.lineinfo[i] = LoadInt()
        end
        f.sizelocvars = LoadInt()
        f.locvars = {}
        for i = 1, f.sizelocvars do
            f.locvars[i] = {}
            f.locvars[i].varname = LoadString()
            f.locvars[i].startpc = LoadInt()
            f.locvars[i].endpc = LoadInt()
        end
        local n = LoadInt()
        for i = 1, n do
            f.upvalues[i].name = LoadString()
        end
    end

    function LoadFunction(f, psource)
        f.source = LoadString()
        if not f.source then
            f.source = psource
        end
        f.linedefined = LoadInt()
        f.lastlinedefined = LoadInt()
        f.numparams = LoadByte()
        f.is_vararg = LoadByte()
        f.maxstacksize = LoadByte()
        LoadCode(f)
        LoadConstants(f)
        LoadUpvalues(f)
        LoadProtos(f)
        LoadDebug(f)
        return f
    end

    local function CheckHeader()
        local LUAC_INT = 0x5678
        local LUAC_NUM = 370.5
        assert(LoadByte() == 0)
        assert(LoadCharN(6) == "\x19\x93\r\n\x1a\n")
        -- int
        assert(string.packsize "i" == LoadByte())
        -- size_t
        assert(string.packsize "T" == LoadByte())
        -- Instruction
        assert(string.packsize "i4" == LoadByte())
        -- lua_Integer
        assert(string.packsize "j" == LoadByte())
        -- lua_Number
        assert(string.packsize "n" == LoadByte())
        assert(LoadInteger() == LUAC_INT)
        assert(LoadNumber() == LUAC_NUM)
    end

    function undump53(cl)
        CheckHeader()
        cl.nupvalues = LoadByte()
        cl.f = {}
        LoadFunction(cl.f, nil)
    end
end

local undump54; do
    local function LoadUnsigned(limit)
        local b
        local x = 0
        limit = limit >> 7
        repeat
            b = LoadByte()
            if x >= limit then
                error("integer overflow")
            end
            x = (x << 7) | (b & 0x7f)
        until ((b & 0x80) ~= 0)
        return x
    end

    local function LoadInt()
        return LoadUnsigned(0x7fffffff)
    end

    local function LoadString()
        local size = LoadUnsigned(math.maxinteger)
        if size == 0 then
            return nil
        end
        return LoadCharN(size - 1)
    end

    local LoadFunction

    local function LoadCode(f)
        f.sizecode = LoadInt()
        f.code = {}
        for i = 1, f.sizecode do
            f.code[i] = unpack "i4"
        end
    end

    local function LoadConstants(f)
        local function makevariant(t, v) return t | (v << 4) end
        local LUA_TNIL     = 0
        local LUA_TBOOLEAN = 1
        local LUA_TNUMBER  = 3
        local LUA_TSTRING  = 4
        local LUA_VNIL     = makevariant(LUA_TNIL, 0)
        local LUA_VFALSE   = makevariant(LUA_TBOOLEAN, 0)
        local LUA_VTRUE    = makevariant(LUA_TBOOLEAN, 1)
        local LUA_VNUMINT  = makevariant(LUA_TNUMBER, 0)
        local LUA_VNUMFLT  = makevariant(LUA_TNUMBER, 1)
        local LUA_VSHRSTR  = makevariant(LUA_TSTRING, 0)
        local LUA_VLNGSTR  = makevariant(LUA_TSTRING, 1)
        f.sizek            = LoadInt()
        f.k                = {}
        for i = 1, f.sizek do
            local t = LoadByte()
            if t == LUA_VNIL then
            elseif t == LUA_VTRUE then
                f.k[i] = true
            elseif t == LUA_VFALSE then
                f.k[i] = false
            elseif t == LUA_VNUMFLT then
                f.k[i] = LoadNumber()
            elseif t == LUA_VNUMINT then
                f.k[i] = LoadInteger()
            elseif t == LUA_VSHRSTR or t == LUA_VLNGSTR then
                f.k[i] = LoadString()
            else
                error("unknown type: "..t)
            end
        end
    end

    local function LoadUpvalues(f)
        f.sizeupvalues = LoadInt()
        f.upvalues = {}
        for i = 1, f.sizeupvalues do
            f.upvalues[i] = {}
            f.upvalues[i].instack = LoadByte()
            f.upvalues[i].idx = LoadByte()
            f.upvalues[i].kind = LoadByte()
        end
    end

    local function LoadProtos(f)
        f.sizep = LoadInt()
        f.p = {}
        for i = 1, f.sizep do
            f.p[i] = {}
            LoadFunction(f.p[i], f.source)
        end
    end

    local function LoadDebug(f)
        f.sizelineinfo = LoadInt()
        f.lineinfo = {}
        for i = 1, f.sizelineinfo do
            f.lineinfo[i] = unpack "b"
        end
        f.sizeabslineinfo = LoadInt()
        f.abslineinfo = {}
        for i = 1, f.sizeabslineinfo do
            f.abslineinfo[i] = {}
            f.abslineinfo[i].pc = LoadInt()
            f.abslineinfo[i].line = LoadInt()
        end
        f.sizelocvars = LoadInt()
        f.locvars = {}
        for i = 1, f.sizelocvars do
            f.locvars[i] = {}
            f.locvars[i].varname = LoadString()
            f.locvars[i].startpc = LoadInt()
            f.locvars[i].endpc = LoadInt()
        end
        local n = LoadInt()
        for i = 1, n do
            f.upvalues[i].name = LoadString()
        end
    end

    function LoadFunction(f, psource)
        f.source = LoadString()
        if not f.source then
            f.source = psource
        end
        f.linedefined = LoadInt()
        f.lastlinedefined = LoadInt()
        f.numparams = LoadByte()
        f.is_vararg = LoadByte()
        f.maxstacksize = LoadByte()
        LoadCode(f)
        LoadConstants(f)
        LoadUpvalues(f)
        LoadProtos(f)
        LoadDebug(f)
        return f
    end

    local function CheckHeader()
        local LUAC_INT = 0x5678
        local LUAC_NUM = 370.5
        assert(LoadByte() == 0)
        assert(LoadCharN(6) == "\x19\x93\r\n\x1a\n")
        -- Instruction
        assert(string.packsize "i4" == LoadByte())
        -- lua_Integer
        assert(string.packsize "j" == LoadByte())
        -- lua_Number
        assert(string.packsize "n" == LoadByte())
        assert(LoadInteger() == LUAC_INT)
        assert(LoadNumber() == LUAC_NUM)
    end

    function undump54(cl)
        CheckHeader()
        cl.nupvalues = LoadByte()
        cl.f = {}
        LoadFunction(cl.f, nil)
    end
end

return function (bytes)
    unpack_pos = 1
    unpack_buf = bytes
    assert(LoadCharN(4) == "\x1bLua")
    local Version = LoadByte()
    local cl = {}
    if Version == 0x53 then
        undump53(cl)
    elseif Version == 0x54 then
        undump54(cl)
    else
        error(("unknown lua version: 0x%x"):format(Version))
    end
    assert(unpack_pos == #unpack_buf + 1)
    assert(cl.nupvalues == cl.f.sizeupvalues)
    return cl, Version
end
