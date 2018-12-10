# vscode-lua-debug

## 安装
在vscode中按Ctrl+P，输入
> ext install lua-debug

## 模块功能
* vscode-lua-debug.exe 代理客户端。在launch模式中，vscode-lua-debug.exe会用debugger.dll创建一个调试器进行调试。在attach模式中，vscode-lua-debug.exe会连接一个远程的调试器(也是由debugger.dll创建的)。无论如何，调试开始后vscode-lua-debug.exe只负责在debugger.dll和vscode之间转发消息。
* debugger.dll 调试器的核心模块。你可以在你的程序中加载debugger.dll并创建调试器，这样vscode就可以通过attach模式进行调试。
* lua53.dll lua核心模块。如果你的程序定制了lua，你可以替换掉它。

## 配置launch.json

1. launch模式，启动lua.exe来执行你的代码。

    * program，lua.exe执行的入口文件 
    * cwd，lua.exe的当前目录
    * stopOnEntry，开始调试时是否先暂停
    * luaexe，指定lua exe的路径，如果不填则会使用调试器自带的luaexe
    * luadll，指定lua dll的路径，如果不填则会加载lua53.dll
    * path，用于初始化package.path
    * cpath，用于初始化package.cpath
    * arg0，lua.exe的命令行参数，用于初始化arg的arg[-n] .. arg[0]
    * arg，lua.exe的命令行参数，用于初始化arg的arg[1] .. arg[n]
    * consoleCoding，lua的标准输出的编码，可选择utf8、ansi、none， 等于none时不会重定向标准输出到vscode
    * sourceMaps，一般不需要，作用同attach模式
    * sourceCoding，作用同attach模式
    * env，修改调试进程的环境变量
    * console，lua.exe在哪个环境下执行，可选择internalConsole，integratedTerminal，externalTerminal
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

2. launch模式，启动一个进程(比如lua.exe或者其他动态链接了luadll的exe)并调试。如果需要调试的目标和lua.exe的行为不一致，可以采用这个模式。

    * runtimeExecutable，进程exe的路径
    * runtimeArgs，启动进程的参数
    * cwd，进程的当前目录，如果不填则是进程exe所在的目录
    * luadll，指定luadll的路径，如果不填会尝试搜索进程exe的导入表，搜索不成功则会加载lua53.dll
    * stopOnEntry，开始调试时是否先暂停
    * sourceMaps，作用同attach模式
    * sourceCoding，作用同attach模式
    * env，修改调试进程的环境变量
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

3. attach模式，调试任意加载了debugger.dll的进程。

    * stopOnEntry，开始调试时是否先暂停
    * ip，远程调试器的ip
    * port，远程调试器的端口
    * sourceMaps，远程代码和本地代码的路径映射
    * sourceCoding，远程代码路径的编码，utf8或者ansi。如果你没修过过lua，windows下默认是ansi。
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

4. attach模式，调试任意加载了lua dll的本地进程。

    * stopOnEntry，开始调试时是否先暂停
    * processId，本地进程的Id，processId和processName只需其中一个。
    * processName，本地进程的Name，使用processName但有多个同名进程时，会失败。
    * sourceMaps，远程代码和本地代码的路径映射
    * sourceCoding，远程代码路径的编码，utf8或者ansi。如果你没修过过lua，windows下默认是ansi。
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

5. 如果你只是使用在本机的远程调试器，你还可以不使用vscode-lua-debug.exe，直接使用vscode连接调试器。只需要加上"debugServer"的参数。例如

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "attach",
            "type": "lua",
            "request": "attach",
            "program": "",
            "stopOnEntry": false,
            "cwd": "${workspaceFolder}",
            "debugServer" : 4278
        }
    ]
}
```

## 远程调试

1. 让debugger.dll和调试目标使用同一个luadll。你可以从以下的方法中选择一个
* 调试目标使用的luadll名也是lua53.dll
* 重新编译debugger.dll，使debugger.dll使用的luadll名和调试目标一致
* 静态链接debugger.dll到调试目标
* 通过debugger.dll的导出函数set_luadll指定调试目标使用的luadll的路径（在其他一切api使用之前）

自动识别
* 如果debugger.dll是通过luadll加载的，debugger.dll可以识别出这种情况，并自己设置luadll的路径。通过*require 'debugger'*来加载，并且是通过lua的默认searcher加载；或者自定义的searcher，但是最后调用dll入口函数是通过luaapi来执行的。
```c
// 可以识别
lua_CFunction f = (lua_CFunction)GetProcAddress(h, "luaopen_debugger");
lua_pushcfunction(L, f);
lua_call(L, 0, 0);

// 不能识别
lua_CFunction f = (lua_CFunction)GetProcAddress(h, "luaopen_debugger");
f(L);
```

2. 在调试目标中加载debugger.dll。debugger.dll目前支持c++导出类和lua模块两套风格的api。为简单起见，这里只说lua模块的加载方式。

确保debugger.dll在package.cpath的搜索范围内，然后执行以下代码
```lua
local dbg = require 'debugger'
dbg:io('listen:0.0.0.0:4278')
dbg:start()
```
此时调试器会监听4278端口，配置好你的vscode，然后用attach模式启动，调试就会被激活。

调试器初始化之后，并不会阻止lua的继续执行，如果你希望调试器不会错过任何东西，你应该等待vscode连接上来，再激活调试。例如
```lua
local dbg = require 'debugger'
dbg:io('listen:0.0.0.0:4278')
dbg:wait()
dbg:start()
```
## 64位的支持

调试器默认会在32位模式下运行。如果你想调试64位的lua，可以选择以下的方式:

1. 远程调试。参考上一章的做法，只需要注意使用64位的`debugger.dll`。
2. 指定`runtimeExecutable`参数。`runtimeExecutable`里提供的为64位进程时，调试器就会在64位下运行。
3. 指定`luaRuntime`参数。

## TODO

* 实现step back
* 自动附加子线程
* 自动附加子进程
* 更加准确的有效行判断
* 重写多线程支持
* 支持wsl

