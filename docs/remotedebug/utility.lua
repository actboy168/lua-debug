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
---尝试安全地退出进程(不一定成功)。
--- * Windows下的行为
---  1. 向主线程发送WM_QUIT消息。
---  2. 如果是console进程，则发送SIGINT信号。
--- * 非Windows下的行为
---  1. 发送SIGINT信号。
---
function utility.closeprocess()
end

return utility
