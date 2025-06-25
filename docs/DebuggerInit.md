### 初始化流程
简单概述几种加载luadebug调试器的方式

前端vscode的参数会被完整传递到前端程序lua-debug代理进程中,该进程负责加载lua-debug.so/.dll到lua虚拟机环境中.

参数传递分为两个阶段,初始化阶段目前只传递了部分参数, 第二阶段调试器加载后DAP协议的initialized消息中传递了所有参数

## Launch
1. 应用程序支持标准lua支持的-e参数
2. 注入器

## Attach
1. 主动连接调试器
2. 注入器

### 主动连接 
在源代码里主动dofile debugger.lua

## Details

### '-e'

通过`DBG 'address[/ansi]/luaversion'` 在字符串中传递启动参数

目前只能传递 address utf8 luaversion 三个参数

### Injector

使用注入器, 加载attach_lua函数作为主入口,最终挂载attach.lua

通过ipc_send_luaversion传递参数

实际上只能传递luaversion这个参数