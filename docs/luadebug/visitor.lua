---@meta

---
---@class refvalue
---指向调试目标的某一个值的引用
---

---
---@alias light-refvalue refvalue | string | number | integer | boolean | nil
---如果需要指向的调试目标的值是一个立即值，则它也是一个立即值，否则是指向它的一个引用
---


---
---@class LuaDebugVisitor
---在调试器VM提供了一套可以访问、修改调试目标的数据以及状态的API。
---
---为了避免对调试目标的影响，luadebug尽可能地不持有调试目标的对象，所以visitor只会记录获取对象的路径，每次当调试器VM需要这个对象时，都会通过路径找到这个对象。由于这种方法和直接持有对象比，比较低效，所以visitor的每个访问API都提供了两个版本，例如getlocal和getlocalv。没有v的访问函数，总是会返回一个userdata，它保存了这个对象路径，每次访问都会遍历路径来找到它。有v的访问函数，如果对象是可以跨VM复制的值，例如number/integer/string/boolean等，那么visitor会将这个值复制到调试器VM，并返回它，如果不能复制，则依然是返回保存了路径的userdata。
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
---@param index integer
---@return string | nil
---@return refvalue
---局部变量。等同于debug.getlocal(frame, index)。
---
function visitor.getlocal(frame, index)
end

---
---@param frame integer
---@param index integer
---@return string | nil
---@return light-refvalue
---局部变量。等同于debug.getlocal(frame, index)。
---
function visitor.getlocalv(frame, index)
end

---
---@param f refvalue
---@param index integer
---@return string | nil
---@return light-refvalue
---上值。等同于debug.getupvalue(f, index)。
---
function visitor.getupvalue(f, index)
end

---
---@param f refvalue
---@param index integer
---@return string | nil
---@return light-refvalue
---上值。等同于debug.getupvalue(f, index)。
---
function visitor.getupvaluev(f, index)
end

---
---@param value refvalue
---@return refvalue
---元表。等同于debug.getmetatable(value)。
---
function visitor.getmetatable(value)
end

---
---@param value refvalue
---@return light-refvalue
---元表。等同于debug.getmetatable(value)。
---
function visitor.getmetatablev(value)
end

---
---@param ud refvalue
---@param index integer | nil
---@return refvalue
---自定义值。等同于debug.getuservalue(ud, index)。
---
function visitor.getuservalue(ud, index)
end

---
---@param ud refvalue
---@param index integer | nil
---@return light-refvalue
---自定义值。等同于debug.getuservalue(ud, index)。
---
function visitor.getuservaluev(ud, index)
end

---
---@param t any
---@param key string
---@return refvalue
---访问表，key的类型必须是字符串。等同于t[key]。
---
function visitor.field(t, key)
end

---
---@param t any
---@param key string
---@return light-refvalue
---访问表，key的类型必须是字符串。等同于t[key]。
---
function visitor.fieldv(t, key)
end

---
---@param t any
---@param i? integer
---@param j? integer
---@return refvalue
---返回table数组部分的值，从i到j。
---返回值是一个数组，tablearray每两个个值分别为value/value(ref)；tablehashv的值为value。
---
function visitor.tablearray(t, i, j)
end

---
---@param t any
---@param i? integer
---@param j? integer
---@return light-refvalue
---返回table数组部分的值，从i到j。
---返回值是一个数组，tablearray每两个个值分别为value/value(ref)；tablehashv的值为value。
---
function visitor.tablearrayv(t, i, j)
end

---
---@param t any
---@param i? integer
---@param j? integer
---@return refvalue[]
---返回table哈希部分的值，从i到j。
---返回值是一个数组，tablehash每三个值分别为key/value/value(ref)；tablehashv每两个值分别为key/value。
---
function visitor.tablehash(t, i, j)
end

---
---@param t any
---@param i? integer
---@param j? integer
---@return light-refvalue[]
---返回table哈希部分的值，从i到j。
---返回值是一个数组，tablehash每三个值分别为key/value/value(ref)；tablehashv每两个值分别为key/value。
---
function visitor.tablehashv(t, i, j)
end

---
---@param t any
---@return integer
---@return integer
---返回table数组部分和哈希部分的长度。
---
function visitor.tablesize(t)
end

---
---@param ud refvalue
---@param offset integer
---@param count integer
---@return string | nil
---读取userdata的内存。
---
function visitor.udread(ud, offset, count)
end

---
---@param ud refvalue
---@param offset integer
---@param data string
---@param allowPartial boolean
---@return integer | boolean
---写入userdata的内存。
---
function visitor.udwrite(ud, offset, data, allowPartial)
end

---
---@param v refvalue | light-refvalue
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
---@param v refvalue | light-refvalue
---@return string
---@return string | number | integer | boolean | nil
---复制v引用的值到调试器VM中，如果v引用的值无法复制，则返回一个"lua_topointer(v)"形式的字符串。
---
function visitor.value(v)
end

---
---@param a refvalue | light-refvalue
---@param b refvalue | light-refvalue
---@return boolean
--- 返回a,b引用的值是否相等
---
function visitor.equal(a, b)
end

---
---@param v refvalue | light-refvalue
---@return string
--- 将v引用的值转为string
---
function visitor.tostring(v)
end

---
---@param v refvalue
---@param new light-refvalue
---@return boolean
---赋值new或者newv引用的值到v引用的值，返回是否成功。
---
function visitor.assign(v, new)
end

---@class visitor.funcinfo
---@field source string?
---@field short_src string?
---@field what string?
---@field name string?
---@field namewhat string?
---@field linedefined integer?
---@field lastlinedefined integer?
---@field currentline integer?
---@field func refvalue?
---@field nparams integer?
---@field istailcall boolean?
---@field ftransfer integer?
---@field ntransfer integer?    
---@field savepc string?        'p'
---@field cuurentpc integer?    'P'

---
---@param frame integer | refvalue
---@param what string
---@param result visitor.funcinfo | nil
---@return visitor.funcinfo
---返回关于一个函数信息的表，如果有result则会将信息填在result中并返回。等同于debug.getinfo(frame, what)。
---
function visitor.getinfo(frame, what, result)
end

---
---@param script string
---@return refvalue
---在调试目标中加载script作为函数，并保存在注册表中。
---
function visitor.load(script)
end

---
---@param f any
---@vararg any
---@return boolean
---@return ...
---执行函数f，如果成功，则第一个返回值是true，随后会将f返回值保存在注册表中并其引用。如果失败，则返回false和错误原因。
---
function visitor.watch(f, ...)
end

---
---@param f any
---@vararg any
---@return boolean
---@return string | number | integer | boolean | nil
---执行函数f，如果成功，则第一个返回值是true，随后会将f的第一个返回值调用`visitor.value`并返回。如果失败，则返回false和错误原因。
---
function visitor.eval(f, ...)
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

---
---@return integer
---等价于`collectgarbage "count"`。
---
function visitor.gccount()
end

---@class visitor.cfunctioninfo
---@field tostring string
---@field file_name string
---@field function_name string
---@field module_name string
---@field line_number string

---
---尝试将C function的转换成具体的符号
---@param fun refvalue
---@return visitor.cfunctioninfo?
---
function visitor.cfunctioninfo(fun)
end

---@class visitor.funcbc
---@field address string
---@field instruction string
---@field line? integer

---
---解析proto的内容
---@param fun integer | refvalue
---@param start? integer
---@param count? integer
---@return visitor.funcbc[]?, string?
function visitor.funcbc(fun, start, count)
end

return visitor
