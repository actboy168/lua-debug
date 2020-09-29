---
---@class RemoteDebugStdio
---用于截获调试目标中的输出。
---
local stdio = {}

---
---@class RemoteDebugRedirect
---
local redirect = {}

---
---@param n integer | nil
---@return string
---读取n个字节的数据。如果没有n，则读到文件结束为止。
---
function redirect:read(n)
end

---
---@return integer
---获取当前有多少数据可读。
---
function redirect:peek()
end

---
---关闭重定向。
---
function redirect:close()
end


---
---@param iotype string
---@return RemoteDebugRedirect
---重定向调试目标的stdout/stderr。
---
function stdio.redirect(iotype)
end

---
---@param enable boolean
---启用`print`事件。
---调试目标每次调用print，都会触发print事件，事件返回值可以决定是否忽略本次的print。
---
function stdio.open_print(enable)
end

---
---@param enable boolean
---启用`iowrite`事件。
---调试目标每次调用io.write/io.stdout:write，都会触发iowrite事件，事件返回值可以决定是否忽略本次的write。
---
function stdio.open_iowrite(enable)
end

return stdio
