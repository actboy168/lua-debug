# vscode-lua-debug

## 安装
在vscode中按Ctrl+P，输入
> ext install lua-debug

## 模块功能
* vscode-debug-client.exe 代理客户端。在launch模式中，vscode-debug-client.exe会用vscode-debug.dll创建一个调试器进行调试。在attach模式中，vscode-debug-client.exe会连接一个远程的调试器(也是由vscode-debug.dll创建的)，vscode-debug-client.exe只负责在vscode-debug.dll和vscode之间转发消息。
* vscode-debug.dll 调试器的核心模块。你可以在你的程序中加载vscode-debug.dll并创建调试器，这样vscode就可以通过attach模式进行调试。
* luacore.dll lua核心模块。如果你的程序定制了lua，你可以替换掉它。

## 配置launch.json

* launch模式，等同于使用lua.exe来执行你的代码。

1. program，lua.exe执行的入口文件 
2. cwd，lua.exe的当前目录
3. stopOnEntry，开始调试时是否先暂停
4. path，用于初始化package.path
5. cpath，用于初始化package.cpath
6. arg，lua.exe的命令行参数，用于初始化arg

* attach模式，调试任意加载了vscode-debug.dll的进程。

1. program，没有用，但由于vscode的bug(?)，你必须有这个参数 
2. cwd，本地代码所在目录
3. stopOnEntry，开始调试时是否先暂停
4. ip，远程调试器的ip
5. port，远程调试器的端口

* 如果你只是使用在本机的远程调试器，你还可以不使用vscode-debug-client.exe，直接使用vscode连接调试器。只需要加上"debugServer"的参数。例如

> {
>     "version": "0.2.0",
>     "debugServer" : 4278,
>     "configurations": [
>         {
>             "name": "attach",
>             "type": "lua",
>             "request": "attach",
>             "program": "",
>             "stopOnEntry": false,
>             "cwd": "${workspaceRoot}",
>         }
>     ]
> }
