local pairs = pairs
local type = type
local next = next
local error = error
local tonumber = tonumber
local tostring = tostring
local utf8_char = utf8.char
local table_concat = table.concat
local string_char = string.char
local string_byte = string.byte
local math_type = math.type
local setmetatable = setmetatable
local Inf = math.huge

local json = {}

json.null = function() end
json.object_mt = {}

-------------------------------------------------------------------------------
-- Encode
-------------------------------------------------------------------------------

local encode

local encode_escape_map = {
    [ "\"" ] = "\\\"",
    [ "\\" ] = "\\\\",
    [ "/" ]  = "\\/",
    [ "\b" ] = "\\b",
    [ "\f" ] = "\\f",
    [ "\n" ] = "\\n",
    [ "\r" ] = "\\r",
    [ "\t" ] = "\\t",
}

local decode_escape_set = {}
local decode_escape_map = {}
for k, v in pairs(encode_escape_map) do
    decode_escape_map[v] = k
    decode_escape_set[string_byte(v, 2)] = true
end

local function encode_escape(c)
    return encode_escape_map[c] or ("\\u%04x"):format(c:byte())
end

local function encode_nil()
    return "null"
end

local function encode_null(val)
    if val == json.null then
        return "null"
    end
    error "cannot serialise function: type not supported"
end

local function encode_string(val)
    return '"' .. val:gsub('[%z\1-\31\127\\"/]', encode_escape) .. '"'
end

local function convertreal(v)
    local g = ('%.16g'):format(v)
    if tonumber(g) == v then
        return g
    end
    return ('%.17g'):format(v)
end

local function encode_number(val)
    if val ~= val or val <= -Inf or val >= Inf then
        error("unexpected number value '" .. tostring(val) .. "'")
    end
    return convertreal(val):gsub(',', '.')
end

local function encode_table(val, stack)
    local first_val = next(val)
    if first_val == nil then
        if getmetatable(val) == json.object_mt then
            return "{}"
        else
            return "[]"
        end
    elseif type(first_val) == 'number' then
        local max = 0
        for k in pairs(val) do
            if math_type(k) ~= "integer" then
                error("invalid table: mixed or invalid key types")
            end
            max = max > k and max or k
        end
        local res = {}
        stack = stack or {}
        if stack[val] then error("circular reference") end
        stack[val] = true
        for i = 1, max do
            res[#res+1] = encode(val[i], stack)
        end
        stack[val] = nil
        return "[" .. table_concat(res, ",") .. "]"
    else
        local res = {}
        stack = stack or {}
        if stack[val] then error("circular reference") end
        stack[val] = true
        for k, v in pairs(val) do
            if type(k) ~= "string" then
                error("invalid table: mixed or invalid key types")
            end
            res[#res+1] = encode_string(k) .. ":" .. encode(v, stack)
        end
        stack[val] = nil
        return "{" .. table_concat(res, ",") .. "}"
    end
end

local type_func_map = {
    [ "nil"      ] = encode_nil,
    [ "table"    ] = encode_table,
    [ "string"   ] = encode_string,
    [ "number"   ] = encode_number,
    [ "boolean"  ] = tostring,
    [ "function" ] = encode_null,
}

encode = function(val, stack)
    local t = type(val)
    local f = type_func_map[t]
    if f then
        return f(val, stack)
    end
    error("unexpected type '" .. t .. "'")
end

function json.encode(val)
    return encode(val)
end

-------------------------------------------------------------------------------
-- Decode
-------------------------------------------------------------------------------

local statusBuf
local statusPos
local statusTop
local statusAry = {}
local statusRef = {}

local function get_word()
    return statusBuf:match("^[^ \t\r\n%]},]*", statusPos)
end

local function next_byte()
    statusPos = statusBuf:find("[^ \t\r\n]", statusPos)
    if statusPos then
        return string_byte(statusBuf, statusPos)
    end
    statusPos = #statusBuf + 1
end

local function getline(str, n)
    local line = 1
    local pos = 1
    while true do
        local f, _, nl1, nl2 = str:find('([\n\r])([\n\r]?)', pos)
        if not f then
            return line, n - pos + 1
        end
        local newpos = f + ((nl1 == nl2 or nl2 == '') and 1 or 2)
        if newpos > n then
            return line, n - pos + 1
        end
        pos = newpos
        line = line + 1
    end
end

local function decode_error(msg)
    error(("ERROR: %s at line %d col %d"):format(msg, getline(msg, statusPos)))
end

local function strchar(chr)
    if chr then
        return string_char(chr)
    end
    return "<eol>"
end

local function parse_unicode_surrogate(s1, s2)
    local n1 = tonumber(s1,  16)
    local n2 = tonumber(s2, 16)
    return utf8_char(0x10000 + (n1 - 0xd800) * 0x400 + (n2 - 0xdc00))
end

local function parse_unicode_escape(s)
    local n1 = tonumber(s,  16)
    return utf8_char(n1)
end

local function parse_string()
    local has_unicode_escape = false
    local has_escape = false
    local i = statusPos
    while true do
        i = statusBuf:find('[\0-\31\\"]', i + 1)
        if not i then
            decode_error "expected closing quote for string"
        end
        local x = string_byte(statusBuf, i)
        if x < 32 then
            statusPos = i
            decode_error "control character in string"
        end
        if x == 92 --[[ "\\" ]] then
            local nx = string_byte(statusBuf, i+1)
            if nx == 117 --[[ "u" ]] then
                if not statusBuf:match("^%x%x%x%x", i+2) then
                    statusPos = i
                    decode_error "invalid unicode escape in string"
                end
                has_unicode_escape = true
                i = i + 5
            else
                if not decode_escape_set[nx] then
                    statusPos = i
                    decode_error("invalid escape char '" .. strchar(nx) .. "' in string")
                end
                has_escape = true
                i = i + 1
            end
        elseif x == 34 --[[ '"' ]] then
            local s = statusBuf:sub(statusPos + 1, i - 1)
            if has_unicode_escape then
                s = s:gsub("\\u([dD][89aAbB]%x%x)\\u([dD][c-fC-F]%x%x)", parse_unicode_surrogate)
                     :gsub("\\u(%x%x%x%x)", parse_unicode_escape)
            end
            if has_escape then
                s = s:gsub("\\.", decode_escape_map)
            end
            statusPos = i + 1
            return s
        end
    end
end

local function parse_number()
    local word = get_word()
    if not (
       word:match '^-?[1-9][0-9]*$'
    or word:match '^-?[1-9][0-9]*[Ee][+-]?[0-9]+$'
    or word:match '^-?[1-9][0-9]*%.[0-9]+$'
    or word:match '^-?[1-9][0-9]*%.[0-9]+[Ee][+-]?[0-9]+$'
    or word:match '^-?0$'
    or word:match '^-?0[Ee][+-]?[0-9]+$'
    or word:match '^-?0%.[0-9]+$'
    or word:match '^-?0%.[0-9]+[Ee][+-]?[0-9]+$'
    ) then
        decode_error("invalid number '" .. word .. "'")
    end
    statusPos = statusPos + #word
    return tonumber(word)
end

local function parse_true()
    if statusBuf:sub(statusPos, statusPos+3) ~= "true" then
        decode_error("invalid literal '" .. get_word() .. "'")
    end
    statusPos = statusPos + 4
    return true
end

local function parse_false()
    if statusBuf:sub(statusPos, statusPos+4) ~= "false" then
        decode_error("invalid literal '" .. get_word() .. "'")
    end
    statusPos = statusPos + 5
    return false
end

local function parse_null()
    if statusBuf:sub(statusPos, statusPos+3) ~= "null" then
        decode_error("invalid literal '" .. get_word() .. "'")
    end
    statusPos = statusPos + 4
    return nil
end

local function parse_array()
    statusPos = statusPos + 1
    local res = {}
    if next_byte() == 93 --[[ "]" ]] then
        statusPos = statusPos + 1
        return res
    end
    statusTop = statusTop + 1
    statusAry[statusTop] = true
    statusRef[statusTop] = res
    return res
end

local function parse_object()
    statusPos = statusPos + 1
    local res = {}
    if next_byte() == 125 --[[ "}" ]] then
        statusPos = statusPos + 1
        return setmetatable(res, json.object_mt)
    end
    statusTop = statusTop + 1
    statusAry[statusTop] = false
    statusRef[statusTop] = res
    return res
end

local char_func_map = {
    [ string_byte '"' ] = parse_string,
    [ string_byte "0" ] = parse_number,
    [ string_byte "1" ] = parse_number,
    [ string_byte "2" ] = parse_number,
    [ string_byte "3" ] = parse_number,
    [ string_byte "4" ] = parse_number,
    [ string_byte "5" ] = parse_number,
    [ string_byte "6" ] = parse_number,
    [ string_byte "7" ] = parse_number,
    [ string_byte "8" ] = parse_number,
    [ string_byte "9" ] = parse_number,
    [ string_byte "-" ] = parse_number,
    [ string_byte "t" ] = parse_true,
    [ string_byte "f" ] = parse_false,
    [ string_byte "n" ] = parse_null,
    [ string_byte "[" ] = parse_array,
    [ string_byte "{" ] = parse_object,
}

local function decode()
    local chr = next_byte()
    local f = char_func_map[chr]
    if not f then
        decode_error("unexpected character '" .. strchar(chr) .. "'")
    end
    return f()
end

local function parse_item()
    local top = statusTop
    local ref = statusRef[top]
    if statusAry[top] then
        ref[#ref+1] = decode()
    else
        local chr = next_byte()
        if chr ~= 34 --[[ '"' ]] then
            decode_error "expected string for key"
        end
        local key = parse_string()
        if next_byte() ~= 58 --[[ ":" ]] then
            decode_error "expected ':' after key"
        end
        statusPos = statusPos + 1
        ref[key] = decode()
    end
    if top == statusTop then
        while true do
            local chr = next_byte(); statusPos = statusPos + 1
            if chr == 44 --[[ "," ]] then
                return
            end
            if statusAry[statusTop] then
                if chr ~= 93 --[[ "]" ]] then
                    decode_error "expected ']' or ','"
                end
            else
                if chr ~= 125 --[[ "}" ]] then
                    decode_error "expected '}' or ','"
                end
            end
            statusTop = statusTop - 1
            if statusTop == 0 then
                return
            end
        end
    end
end

function json.decode(str)
    if type(str) ~= "string" then
        error("expected argument of type string, got " .. type(str))
    end
    statusBuf = str
    statusPos = 1
    statusTop = 0
    local res = decode()
    while statusTop > 0 do
        parse_item()
    end
    if next_byte() ~= nil then
        decode_error "trailing garbage"
    end
    return res
end

return json
