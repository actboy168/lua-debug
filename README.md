# lua-debug

[![Build Status](https://github.com/actboy168/lua-debug/workflows/build/badge.svg)](https://github.com/actboy168/lua-debug/actions?workflow=build)

## Requirements

* Lua 5.1 - 5.4 or luajit (thanks [@fesily](https://github.com/fesily))
* Platform: Windows, macOS, Linux, Android, NetBSD, FreeBSD

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

1. Install [luamake](https://github.com/actboy168/luamake)
``` bash
git clone https://github.com/actboy168/luamake
pushd luamake
git submodule init
git submodule update
.\compile\install.bat(msvc)
./compile/install.sh (other)
popd
```

2. Clone repo.
``` bash
git clone https://github.com/actboy168/lua-debug
cd lua-debug
git submodule init
git submodule update
```

3. Download deps.
``` bash
luamake lua compile/download_deps.lua
```

4. Build
```
luamake -mode release
```

## Install to VSCode

1. Install extension `actboy168.lua-debug` and `actboy168.extension-path`
2. Open [repo](https://github.com/actboy168/lua-debug) in VSCode
3. Run task：Copy Publish

## Todo

* `thunk` support arm64.
* Use [lua-epoll](https://github.com/actboy168/lua-epoll) instead of select.
* iOS example.
