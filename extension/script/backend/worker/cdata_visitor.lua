local rdebug = require("remotedebug.visitor")
local function readfile(filename)
    local fullpath = assert(package.searchpath(filename, package.path))
    local f = assert(io.open(fullpath))
    local str = f:read 'a'
    f:close()
    return str
end

local handler = assert(rdebug.load(readfile "backend.worker.eval.ffi_reflection"))

local visitor = {}
function visitor.shorttypename(v)
    local ok, name = rdebug.eval(handler, "shorttypename", v)
    return ok and name or nil
end

function visitor.shortvalue(v)
    local ok, value = rdebug.eval(handler, "shortvalue", v)
    return ok and value or nil
end

function visitor.canextand(v)
    local ok, canextand = rdebug.eval(handler, "canextand", v)
    return ok and canextand or false
end

function visitor.typename(v)
    local ok, name = rdebug.eval(handler, "typename", v)
    return ok and name or nil
end

function visitor.what(v)
    local ok, name = rdebug.eval(handler, "what", v)
    return ok and name or nil
end

function visitor.member(v, index)
    local ok, v1, v2 = rdebug.watch(handler, "member", v, index)
    if not ok then return end
    return v1, v2
end

function visitor.annotated_member(reflect, index, v)
    local ok, v1, v2 = rdebug.watch(handler, "annotated_member", reflect, index, v)
    if not ok then return end
    return v1, v2
end

return visitor