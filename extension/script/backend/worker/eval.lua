local rdebug = require 'remotedebug.visitor'
local luaver = require 'backend.worker.luaver'

local readfile = package.readfile
if not readfile then
    function readfile(filename)
        local fullpath = assert(package.searchpath(filename, package.path))
        local f = assert(io.open(fullpath))
        local str = f:read 'a'
        f:close()
        return str
    end
end

local eval_readwrite = assert(rdebug.load(readfile 'backend.worker.eval.readwrite'))
local eval_readonly  = assert(rdebug.load(readfile 'backend.worker.eval.readonly'))
local eval_verify    = assert(rdebug.load(readfile 'backend.worker.eval.verify'))

local m = {}

function m.readwrite(expression, frameId)
    return rdebug.watch(eval_readwrite, expression, frameId)
end

function m.readonly(expression, frameId)
    return rdebug.watch(eval_readonly, expression, frameId)
end

function m.eval(expression, level, symbol)
    return rdebug.eval(eval_readonly, expression, level, symbol)
end

function m.verify(expression)
    return rdebug.eval(eval_verify, expression, 0)
end

local function generate(name, init)
    m[name] = function (...)
        local f = init()
        m[name] = f
        return f(...)
    end
end

generate("dump", function ()
    if luaver.LUAVERSION <= 52 then
        local compat_dump = assert(load(readfile 'backend.worker.eval.dump'))
        return function (content)
            local res, err = compat_dump(content)
            if res then
                return true, res
            end
            return false, err
        end
    else
        local eval_dump = assert(rdebug.load(readfile 'backend.worker.eval.dump'))
        return function (content)
            return rdebug.eval(eval_dump, content, 0)
        end
    end
end)

return m
