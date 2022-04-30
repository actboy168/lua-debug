# lua-debug

[![Build Status](https://github.com/actboy168/lua-debug/workflows/build/badge.svg)](https://github.com/actboy168/lua-debug/actions?workflow=build)

## Requirements

* Lua 5.1 - 5.4 or luajit (thanks [@fesily](https://github.com/fesily))
* Platform: Windows, macOS, Linux, Android

## Feature

* Breakpoints
* Function breakpoints
* Conditional breakpoints
* Hit Conditional breakpoints
* Step over, step in, step out
* Watches
* Evaluate expressions
* Exception
* Remote debugging
* Support WSL

## Build

1. Install extension `actboy168.lua-debug` and `actboy168.extension-path`
2. Build [luamake](https://github.com/actboy168/luamake)
3. Open [repo](https://github.com/actboy168/lua-debug) in VSCode
4. Run task：Rebuild
5. Run task：Copy Publish

## Todo

* `thunk` support arm64.
* Use [lua-epoll](https://github.com/actboy168/lua-epoll) instead of select.
* iOS example.
