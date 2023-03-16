---@meta

---
---@class LuaDebugUtility
---
local utility = {}

---
---@return string
---获取当前路径
---
function utility.fs_current_path()
end

---
---@return string
---获取exe所在目录
---
function utility.fs_program_path()
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
