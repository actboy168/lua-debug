---@meta

---
---@class LuaDebugUtility
---
local utility = {}

---
--- * Windows下的行为
---  1. 向主线程发送WM_QUIT消息。
--- * 非Windows下的行为
---  1. 什么都不做
---
function utility.closewindow()
end

---
--- 发送SIGINT信号。
---
function utility.closeprocess()
end

return utility
