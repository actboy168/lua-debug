---
---@class refvalue
---指向调试目标的某一个值的指针或者是一个立即值(string/number/boolean/nil)
---

---
---@class RemoteDebugVisitor
---在调试器VM提供了一套可以访问、修改调试目标的数据以及状态的API。
---
---为了避免对调试目标的影响，remotedebug尽可能地不持有调试目标的对象，所以visitor只会记录获取对象的路径，每次当调试器VM需要这个对象时，都会通过路径找到这个对象。由于这种方法和直接持有对象比，比较低效，所以visitor的每个访问API都提供了两个版本，例如getlocal和getlocalv。没有v的访问函数，总是会返回一个userdata，它保存了这个对象路径，每次访问都会遍历路径来找到它。有v的访问函数，如果对象是可以跨VM复制的值，例如number/string/boolean等，那么visitor会将这个值复制到调试器VM，并返回它，如果不能复制，则依然是返回保存了路径的userdata。
---
---如果你只需要访问一些对象，使用v的访问函数就只够了，它会更加高效。只有当你需要修改对象时，才需要用没有v的访问函数。
---
local visitor = {}

---
---@type refvalue
---全局表。等同于_G。
---
visitor._G = nil

---
---@type refvalue
---注册表。等同于debug.getregistry()。
---
visitor._REGISTRY = nil


---
---@param frame integer
---@return refvalue
---指定帧的函数。等同于debug.getinfo(frame, "f")。
---
function visitor.getfunc(frame)
end

---
---@param frame integer
---@param index integer
---@return refvalue
---局部变量。等同于debug.getlocal(frame, index)。
---
function visitor.getlocal(frame, index)
end
visitor.getlocalv = visitor.getlocal

---
---@param f refvalue
---@param index integer
---@return refvalue
---上值。等同于debug.getupvalue(f, index)。
---
function visitor.getupvalue(f, index)
end
visitor.getupvaluev = visitor.getupvalue

---
---@param value refvalue
---@return refvalue
---元表。等同于debug.getmetatable(value)。
---
function visitor.getmetatable(value)
end
visitor.getmetatablev = visitor.getmetatable

---
---@param ud refvalue
---@param index integer | nil
---@return refvalue
---自定义值。等同于debug.getuservalue(ud, index)。
---
function visitor.getuservalue(ud, index)
end
visitor.getuservaluev = visitor.getuservalue

---
---@param t refvalue
---@param key integer
---@return refvalue
---访问表，key的类型必须是整数。等同于t[key]。
---
function visitor.index(t, key)
end
visitor.indexv = visitor.index

---
---@param t refvalue
---@param key string
---@return refvalue
---访问表，key的类型必须是字符串。等同于t[key]。
---
function visitor.field(t, key)
end
visitor.fieldv = visitor.field

---
---@param idx integer | nil
---@return refvalue
---返回栈上的对象,没有idx时返回栈上有几个对象，有idx时返回栈上第idx个对象。等同于lua_gettop/lua_pushvalue。
---
function visitor.getstack(idx)
end

---
---@param t refvalue
---@param limit integer | nil
---@return refvalue[]
---返回table哈希部分的值，返回最多limit个，如果没有limit则返回所有的值。
---返回值是一个数组，tablehash每三个值分别为key/value/value(ref)；tablehashv每两个值分别为key/value。
---
function visitor.tablehash(t, limit)
end
visitor.tablehashv = visitor.tablehash


---
---@param t refvalue
---@return integer
---@return integer
---返回table数组部分和哈希部分的长度。
---
function visitor.tablesize(t)
end

---
---@param t refvalue
---@param i integer
---@return string
---@return integer
---搜索下一个类型为string的key。
---
function visitor.tablekey(t, i)
end

---
---@param v refvalue
---@return string | integer | number | boolean | nil
---复制v引用的值到调试器VM中，如果v引用的值无法复制，则返回一个"type: lua_topointer(v)"形式的字符串。
---
function visitor.value(v)
end

---
---@param v refvalue
---@param new refvalue
---@return boolean
---赋值new或者newv引用的值到v引用的值，返回是否成功。
---
function visitor.assign(v, new)
end

---
---@param v refvalue
---@return string
---返回v引用的值的类型，和type(v)略有不同。
---  * 如果type(v)=="number", 则会返回math.type(v)，如果调试目标低于5.3则返回"float"。
---  * LUA_TLIGHTUSERDATA会返回"lightuserdata"。
---  * LUA_TNONE会返回"unknown"。
---  * C Function会返回"c function"。
---  * 其余情况返回type(v)。
---
function visitor.type(v)
end

---
---@param frame integer
---@param what string
---@param result table | nil
---@return table
---返回关于一个函数信息的表，如果有result则会将信息填在result中并返回。等同于debug.getinfo(frame, what)。
---
function visitor.getinfo(frame, what, result)
end

---
---@param script string
---@return refvalue
---在调试目标中加载script作为函数，并保存在注册表中。
---
function visitor.reffunc(script)
end

---
---@param f refvalue
---@vararg any
---@return boolean
---@return ...
---执行函数f，如果成功，则第一个返回值是true，随后会将f返回值保存在注册表中并其引用。如果失败，则返回false和错误原因。
---
function visitor.watch(f,...)
end

---
---@param f refvalue
---@vararg any
---@return boolean
---@return string | integer | number | boolean | nil
---执行函数f，如果成功，则第一个返回值是true，随后会将f的第一个返回值调用`visitor.value`并返回。如果失败，则返回false和错误原因。
---
function visitor.eval(f,...)
end

---
---清除所有visitor.watch生成的引用。
---
function visitor.cleanwatch()
end

---
---@param co refvalue
---@return string
---co不是thread返回"invalid"，否则返回coroutine.status(co)。
---
function visitor.costatus(co)
end

return visitor
