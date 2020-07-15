# 移植指南

如果你打算使用自己的lua，而非调试器提供的，则需要注意以下几点。

## coroutine支持

如果你需要调试coroutine内的代码，则需要让你的lua可以抛出THREAD的调试事件，具体可以参考调试器自带lua的实现(搜索THREAD)。

此外，你也可以在lua端自己抛这个事件而无需修改lua。例如
``` lua
local rdebug = require "remotedebug"

local rawcoroutineresume = coroutine.resume
local rawcoroutinewrap   = coroutine.wrap
local function coreturn(co, ...)
    rdebug.event("thread", co, 1)
    return ...
end
function coroutine.resume(co, ...)
    rdebug.event("thread", co, 0)
    return coreturn(co, rawcoroutineresume(co, ...))
end
function coroutine.wrap(f)
    local wf = rawcoroutinewrap(f)
    local _, co = debug.getupvalue(wf, 1)
    return function(...)
        rdebug.event("thread", co, 0)
        return coreturn(co, wf(...))
    end
end
```

## error支持

如果你需要捕获lua抛出的错误，则需要让你的lua可以抛出EXCEPTION的调试事件，具体可以参考调试器自带lua的实现(搜索EXCEPTION)。

此外，你也可以在lua端自己抛这个事件而无需修改lua。例如
``` lua
local rdebug = require "remotedebug"

local rawxpcall = xpcall
function pcall(f, ...)
    return rawxpcall(f,
        function(msg)
            rdebug.event("exception", msg)
            return msg
        end,
    ...)
end
function xpcall(f, msgh, ...)
    return rawxpcall(f,
        function(msg)
            rdebug.event("exception", msg)
            return msgh and msgh(msg) or msg
        end
    , ...)
end
```

## 使用你的lua重写编译remotedebug

remotedebug引用了非公开的lua头文件lstate.h，如果你的lua有修改过它，则需要重新编译remotedebug。


## update优化

默认的情况下，调试器会在没有断点时加载一个count hook，以便调试器有时机可以响应GUI的请求（新的断点/暂停等）。但这也会影响到调试器没有断点时的运行效率，所以你可以将这个行为关闭，由lua在合适周期抛出update事件，从而将调试器对运行效率的影响降到最低(几乎可以忽略不记)。

update事件，只会影响调试运行过程中的执行GUI操作的响应时间（一般就是新的断点和暂停），所以一般不大于0.2秒就足够了。

关闭方法：搜索openupdate，将其去掉。
