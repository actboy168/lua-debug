# remotedebug文档

> 为什么需要remotedebug?

remotedebug设计的首要目标是尽可能减少调试器对调试目标的影响。虽然lua-debug看起来和其它Lua调试器一样大部分的代码是用Lua编写，但这些Lua没有在调试目标的VM上执行。remotedebug内嵌了一个Lua 5.4的VM，无论调试目标是Lua5.1或者Lua5.4，调试器的代码始终是跑在remotedebug内嵌的Lua 5.4上。调试器只能通过remotedebug提供的API读取和修改调试目标的数据。这样可以最大限度地降低调试器对调试目标的影响。

因此remotedebug可以被调试目标和调试器的代码分别加载，但对它们提供的API并不相同。

## host
在调试目标中管理调试器VM。

```lua
local rdebug = require "remotedebug"
```
#### rdebug.start(script: string, init: function|nil)
启动调试器VM，执行script的代码。init是可选参数，可以传入一个不带upvalue的c function。它将会作为执行script时的第一个参数传入。

#### rdebug.clear()
关闭调试器VM。

#### rdebug.event(name: string): boolean|nil
触发一个事件给调试器VM。它会等调试器VM处理完毕才会返回。如果失败或者返回值不是boolean，则返回nil，否则返回事件的返回值。

#### rdebug.a2u(str: string): string
字符串编码转换，从ansi到utf8。

## visitor
在调试器VM提供了一套可以访问、修改调试目标的数据以及状态的API。
```lua
local visitor = require "remotedebug.visitor"
```

为了避免对调试目标的影响，remotedebug尽可能地不持有调试目标的对象，所以visitor只会记录获取对象的路径，每次当调试器VM需要这个对象时，都会通过路径找到这个对象。由于这种方法和直接持有对象比，比较低效，所以visitor的每个访问API都提供了两个版本，例如getlocal和getlocalv。没有v的访问函数，总是会返回一个userdata，它保存了这个对象路径，每次访问都会遍历路径来找到它。有v的访问函数，如果对象是可以跨VM复制的值，例如number/string/boolean等，那么visitor会将这个值复制到调试器VM，并返回它，如果不能复制，则依然是返回保存了路径的userdata。

如果你只需要访问一些对象，使用v的访问函数就只够了，它会更加高效。只有当你需要修改对象时，才需要用没有v的访问函数。

#### visitor._G: refvalue
全局表。等同于_G。

#### visitor._REGISTRY: refvalue
注册表。等同于debug.getregistry()。

#### visitor.getlocal/getlocalv(frame: integer, index: integer): refvalue
局部变量。等同于debug.getlocal(frame, index)。

#### visitor.getfunc(frame: integer): refvalue
指定帧的函数。等同于debug.getinfo(frame, "f")。

#### visitor.getupvalue/getupvaluev(f: refvalue, index: integer): refvalue
上值。等同于debug.getupvalue(f, index)。

#### visitor.getmetatable/getmetatablev(value: refvalue): refvalue
元表。等同于debug.getmetatable(value)。

#### visitor.getuservalue/getuservaluev(ud: refvalue, index: integer|nil): refvalue
自定义值。等同于debug.getuservalue(ud, index)。

#### visitor.index/indexv(t: refvalue, key: integer): refvalue
访问表，key的类型必须是整数。等同于t[key]。

#### visitor.field/fieldv(t: refvalue, key: string): refvalue
访问表，key的类型必须是字符串。等同于t[key]。

#### visitor.getstack(idx: integer|nil): refvalue
返回栈上的对象,没有idx时返回栈上有几个对象，有idx时返回栈上第idx个对象。等同于lua_gettop/lua_pushvalue。

#### visitor.tablehash/tablehashv(t: refvalue, limit: integer|nil): {refvalue}
返回table哈希部分的值，返回最多limit个，如果没有limit则返回所有的值。
返回值是一个数组，tablehash每三个值分别为key/value/value(ref)；tablehashv每两个值分别为key/value。

#### visitor.tablesize(t: refvalue): integer，integer
返回table数组部分和哈希部分的长度。

#### visitor.tablekey(t: refvalue, i: integer): string,integer
搜索下一个类型为string的key。

#### visitor.value(v: refvalue): string|integer|number|boolean|nil
复制v引用的值到调试器VM中，如果v引用的值无法复制，则返回一个"type: topointer"形式的字符串。

#### visitor.assign(v: refvalue, new: refvalue): boolean
赋值new或者newv引用的值到v引用的值，返回是否成功。

#### visitor.type(v: refvalue): string
返回v引用的值的类型，和type(v)略有不同。
* 如果type(v)=="number", 则会返回math.type(v)，如果调试目标低于5.3则返回"float"。
* LUA_TLIGHTUSERDATA会返回"lightuserdata"。
* LUA_TNONE会返回"unknown"。
* C Function会返回"c function"。
* 其余情况返回type(v)。

#### visitor.getinfo(frame: integer, what: string, result: table|nil): table
返回关于一个函数信息的表，如果有result则会将信息填在result中并返回。等同于debug.getinfo(frame, what)。

#### visitor.reffunc(script: string): refvalue
在调试目标中加载script作为函数，并保存在注册表中。

#### visitor.watch(f: refvalue, ...): boolean, ...
执行函数f，如果成功，则第一个返回值是true，随后会将f返回值保存在注册表中并其引用。如果失败，则返回false和错误原因。

#### visitor.eval(f: refvalue, ...): boolean, value
执行函数f，如果成功，则第一个返回值是true，随后会将f的第一个返回值调用`visitor.value`并返回。如果失败，则返回false和错误原因。

#### visitor.cleanwatch()
清除所有visitor.watch生成的引用。

#### visitor.costatus(co: refvalue): string
co不是thread返回"invalid"，否则返回coroutine.status(co)。

## hookmgr
在调试器VM提供了一个hook管理器。理论上它可以用visitor提供的API，完全由Lua实现。但出于对性能的考虑，所以将hook的管理有C++实现，这也是remotedebug中唯一考虑了性能的一个模块，
```lua
local hookmgr = require "remotedebug.hookmgr"
```

#### hookmgr.init(callback: function)
初始化hookmgr，并注册一个回调函数。当有事件被触发时，会调用回调函数。事件可以是rdebug.probe/rdebug.event触发的，也可以是hookmgr内部触发的内置事件。

* `newproto` proto也就是函数原型，每次调试器遇到新的proto就会触发这个事件。返回值需要告诉调试器这个proto是否包含断点，然后调试器会自动调用hookmgr.break_add或hookmgr.break_del。
* `bp` 执行有断点的proto时会触发，需要自己检查是否命中了行号。
* `step` 满足单步状态时触发。
* `funcbp` 每次进入一个函数时会触发。需要用hookmgr.funcbp_open激活。
* `update` 每隔一段时间触发。需要用hookmgr.update_open激活。
* `r_exception` 每次触发非内存错误时触发。需要用hookmgr.exception_open激活。需要补丁支持。
* `r_thread` 每次进入或退出thread会触发。需要用hookmgr.thread_open激活。需要补丁支持。
* `panic` 进入panic时触发。

#### hookmgr.sethost(co: thread)
设置当前调试的协程为co。

#### hookmgr.gethost(): thread
获取当前调试的协程。

#### hookmgr.updatehookmask(co: thread)
更新指定协程的hookmask。(因为Lua不会帮你更新)

#### hookmgr.stacklevel(): integer
获取当前的栈层级。

#### hookmgr.break_add(proto: lightuserdata)
设置proto有断点。

#### hookmgr.break_del(proto: lightuserdata)
设置proto没断点。

#### hookmgr.break_open()
启用`bp`事件。

#### hookmgr.break_closeline()
仅在本次函数调用中关闭`bp`事件。

#### hookmgr.funcbp_open(enable: boolean)
启用`funcbp`事件。

#### hookmgr.step_in()
步入。

#### hookmgr.step_out()
步出。

#### hookmgr.step_over()
步过。

#### hookmgr.step_cancel()
取消`hookmgr.step_in/out/over`的状态。

#### hookmgr.update_open(enable: boolean)
启用`update`事件。

#### hookmgr.exception_open(enable: boolean)
启用`r_exception`事件。

#### hookmgr.thread_open(enable: boolean)
启用`r_thread`事件。

## stdio
用于截获调试目标中的输出。
```lua
local stdio = require "remotedebug.stdio"
```

#### stdio.redirect(iotype: string): redirect
重定向调试目标的stdout/stderr。

#### redirect:read(n: integer|nil): string
读取n个字节的数据。如果没有n，则读到文件结束为止。

#### redirect:peek(): integer
获取当前有多少数据可读。

#### redirect:close()
关闭重定向。

#### stdio.open_print(enable: boolean)
调试目标每次调用print，都会触发print事件，返回值可以决定是否忽略本次的print。

#### stdio.open_iowrite(enable: boolean)
调试目标每次调用io.write/io.stdout:write，都会触发iowrite事件，返回值可以决定是否忽略本次的write。

## utility
```lua
local utility = require "remotedebug.utility"
```

#### utility.fs_absolute(path: string): string
把一个路径变为绝对路径，并且把它规则化。

#### utility.closeprocess()
尝试安全地退出进程(不一定成功)。
> Windows下的行为
1. 向主线程发送WM_QUIT消息。
2. 如果是console进程，则发送SIGINT信号。
> 非Windows下的行为
1. 发送SIGINT信号。

## thread
导入了[bee.lua](https://github.com/actboy168/bee.lua)的`bee.thread`，请参考bee.lua的文档。
```lua
local thread = require "remotedebug.thread"
```

## socket
导入了[bee.lua](https://github.com/actboy168/bee.lua)的`bee.socket`，请参考bee.lua的文档。
```lua
local socket = require "remotedebug.socket"
```
