---
---@class RemoteDebugHookmgr
---在调试器VM提供了一个hook管理器。理论上它可以用visitor提供的API，完全由Lua实现。但出于对性能的考虑，所以将hook的管理有C++实现，这也是remotedebug中唯一考虑了性能的一个模块。
---
local hookmgr = {}

---
---@param callback fun(name:string,...):boolean|nil
---初始化hookmgr，并注册一个回调函数。当有事件被触发时，会调用回调函数。事件可以是rdebug.probe/rdebug.event触发的，也可以是hookmgr内部触发的内置事件。
---* `newproto` proto也就是函数原型，每次调试器遇到新的proto就会触发这个事件。返回值需要告诉调试器这个proto是否包含断点，然后调试器会自动调用hookmgr.break_add或hookmgr.break_del。
---* `bp` 执行有断点的proto时会触发，需要自己检查是否命中了行号。
---* `step` 满足单步状态时触发。
---* `funcbp` 每次进入一个函数时会触发。需要用hookmgr.funcbp_open激活。
---* `update` 每隔一段时间触发。需要用hookmgr.update_open激活。
---* `exception` 每次触发非内存错误时触发。需要用hookmgr.exception_open激活。需要补丁支持。
---* `thread` 每次进入或退出thread会触发。需要用hookmgr.thread_open激活。需要补丁支持。
---
function hookmgr.init(callback)
end

---
---@param co thread
---设置当前调试的协程为co。
---
function hookmgr.sethost(co)
end

---
---@return thread
---获取当前调试的协程。
---
function hookmgr.gethost(co)
end

---
---@param co thread
---更新指定协程的hookmask。(因为Lua不会帮你更新)
---
function hookmgr.updatehookmask(co)
end

---
---@return integer
---获取当前的栈层级。
---
function hookmgr.stacklevel()
end

---
---@param proto lightuserdata
---设置proto有断点。
---
function hookmgr.break_add(proto)
end

---
---@param proto lightuserdata
---设置proto没断点。
---
function hookmgr.break_del(proto)
end

---
---启用`bp`事件。
---
function hookmgr.break_open(proto)
end

---
---仅在本次函数调用中关闭`bp`事件。
---
function hookmgr.break_closeline(proto)
end

---
---@param enable boolean
---启用`funcbp`事件。
---
function hookmgr.funcbp_open(enable)
end

---
---步入。
---
function hookmgr.step_in()
end

---
---步出。
---
function hookmgr.step_out()
end

---
---步过。
---
function hookmgr.step_over()
end

---
---取消`step_in/step_out/step_over`的状态。
---
function hookmgr.step_cancel()
end

---
---@param enable boolean
---启用`update`事件。
---
function hookmgr.update_open(enable)
end

---
---@param enable boolean
---启用`exception`事件。
---
function hookmgr.exception_open(enable)
end

---
---@param enable boolean
---启用`thread`事件。
---
function hookmgr.thread_open(enable)
end

return hookmgr
