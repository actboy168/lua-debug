local c = {}

local tag = nil

function c.setTag(t)
    if tag and tag ~= t then
        error(('luadebugger worker has set tag: [%s] -> [%s]'):format(tag, t))
    end
    tag = t
end

local function randomTag()
    if not tag then
        tag = tostring(math.random(9999))
    end
end

function c.getChannelKeyMaster()
    randomTag()
    return 'DbgMaster'..(tag or '')
end

function c.getChannelKeyWorker(id)
    randomTag()
    return ('DbgWorker(%s)'..(tag or '')):format(id)
end

return c
