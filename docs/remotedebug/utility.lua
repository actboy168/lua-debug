---
---@class RemoteDebugUtility
---
local utility = {}

---
---@param path string
---@return string
---把一个路径变为绝对路径，并且把它规则化。
---
function utility.fs_absolute(path)
end

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

---
---@param str string
---@return string
---将utf8字符串转为ansi。
---仅在Windows下有效。
---
function utility.u2a(str)
end

---
---@param str string
---@return string
---将ansi字符串转为utf8。
---仅在Windows下有效。
---
function utility.a2u(str)
end

return utility
