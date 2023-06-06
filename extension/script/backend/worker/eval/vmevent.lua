local jit = require("jit")
if jit.version_num ~= 20100 then
    return "LuaJIT core/library version mismatch"
end
--TODO:检查是否有luajit profile被启用了
local active = false
local record_count = 0
local record_updeta_count = 1024

---@module "script.debugger"
local luadebugger = debug.getregistry()["lua-debug"]

local function update_trace()
    luadebugger:event("update")
end
local function update_record()
    record_count = record_count + 1
    if record_count < record_updeta_count then
        return
    end
    luadebugger:event("update")
    record_count = 0
end
local function update_texit()
    luadebugger:event("update")
end

local function on()
    jit.attach(update_trace, "trace")
    jit.attach(update_record, "record")
    jit.attach(update_texit, "texit")
    active = true
end
local function off()
    jit.attach(update_trace)
    jit.attach(update_record)
    jit.attach(update_texit)
    active = false
end

return function (enable)
    if enable and not active then
        on()
    elseif active then
        off()
    end
end
