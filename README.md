# lua-debug


[![Build Status](https://github.com/actboy168/lua-debug/workflows/build/badge.svg)](https://github.com/actboy168/lua-debug/actions?workflow=build)

## 安装
在vscode中按Ctrl+P，输入
> ext install actboy168.lua-debug

## 配置launch.json

1. launch模式，启动lua.exe来执行你的代码。

    * program，lua.exe执行的入口文件 
    * cwd，lua.exe的当前目录
    * stopOnEntry，开始调试时是否先暂停
    * luaexe，指定lua exe的路径，如果不填则由luaVersion和luaArch决定
    * luaVersion，指定lua运行时的版本。可选项有5.1/5.2/5.3/5.4。
    * luaArch，指定lua运行时的指令集，仅windows有效。可选项有x86/x86_64。
    * path，用于初始化package.path
    * cpath，用于初始化package.cpath
    * arg0，lua.exe的命令行参数，用于初始化arg的arg[-n] .. arg[0]
    * arg，lua.exe的命令行参数，用于初始化arg的arg[1] .. arg[n]
    * consoleCoding，lua的标准输出的编码，可选择utf8、ansi、none， 等于none时不会重定向标准输出到vscode
    * sourceMaps，一般不需要，作用同attach模式
    * sourceCoding，作用同attach模式
    * outputCapture，需要捕获哪些输出到调试控制台。可选项有print,io.write,stdout,stderr。
    * env，修改调试进程的环境变量
    * console，lua.exe在哪个环境下执行，可选择internalConsole，integratedTerminal，externalTerminal
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

2. launch模式，启动一个进程(比如lua.exe或者其他动态链接了luadll的exe)并调试。如果需要调试的目标和lua.exe的行为不一致，可以采用这个模式。

    * runtimeExecutable，进程exe的路径
    * runtimeArgs，启动进程的参数
    * cwd，进程的当前目录，如果不填则是进程exe所在的目录
    * stopOnEntry，开始调试时是否先暂停
    * path，用于初始化package.path
    * cpath，用于初始化package.cpath
    * sourceMaps，作用同attach模式
    * sourceCoding，作用同attach模式
    * outputCapture，作用同上
    * env，修改调试进程的环境变量
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

3. attach模式，调试任意加载了调试器的进程。

    * stopOnEntry，开始调试时是否先暂停
    * address，远程调试器的地址
    * client，用connect还是listen的方式使用address
    * sourceMaps，远程代码和本地代码的路径映射
    * sourceCoding，远程代码路径的编码，utf8或者ansi。如果你没修过过lua，windows下默认是ansi。
    * outputCapture，作用同上
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。

4. attach模式，调试任意加载了lua dll的本地进程。

    * stopOnEntry，开始调试时是否先暂停
    * processId，本地进程的Id，processId和processName只需其中一个。
    * processName，本地进程的Name，使用processName但有多个同名进程时，会失败。
    * sourceMaps，远程代码和本地代码的路径映射
    * sourceCoding，远程代码路径的编码，utf8或者ansi。如果你没修过过lua，windows下默认是ansi。
    * outputCapture，作用同上
    * skipFiles，让调试器忽略某些脚本，例如, ["std/\*", test/\*/init.lua]。


## TODO

* 实现step back
* 自动附加子线程
* 自动附加子进程
* 数据断点
* 选取进程列表
