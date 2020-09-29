---
---@class RemoteDebug
---在调试目标中管理调试器VM。
---
local rdebug = {}

---@param script string
---@param init function | nil
---启动调试器VM，执行script的代码。init是可选参数，可以传入一个不带upvalue的c function。它将会作为执行script时的第一个参数传入。
---
function rdebug.start(script, init)
end

---
---关闭调试器VM。
---
function rdebug.clear()
end

---
---@param name string
---@return boolean | nil
---触发一个事件给调试器VM。它会等调试器VM处理完毕才会返回。如果失败或者返回值不是boolean，则返回nil，否则返回事件的返回值。
---
function rdebug.event(name)
end

---
---@param str string
---@return string
---字符串编码转换，从ansi到utf8。
---
function rdebug.a2u(str)
end

return rdebug
