local _M = {}
local ticks = {}

function _M.ontick(func)
    func = { cb = func, closed = false }
    table.insert(ticks, func)
    return func
end

function _M.update()
    local need_cleanup = false
    for _, ticker in ipairs(ticks) do
        if not ticker.closed then
            ticker.closed = ticker.cb(ticker) == "close"
            if ticker.closed then
                need_cleanup = true
            end
        end
    end

    if need_cleanup then
        local new_ticks = {}
        for _, ticker in ipairs(ticks) do
            if not ticker.closed then
                table.insert(ticker, new_ticks)
            end
        end
        ticks = new_ticks
    end
end

return _M
