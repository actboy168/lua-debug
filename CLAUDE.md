# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建系统

使用 [luamake](https://github.com/actboy168/luamake) 作为构建工具（基于 Lua）。

```bash
# 先安装 luamake（见 README），然后：
luamake lua compile/download_deps.lua   # 下载依赖
luamake -mode release                    # 构建
luamake test                             # 仅构建测试
```

## 架构

这是一个 VS Code 调试器扩展，支持调试 Lua 5.1–5.5 和 LuaJIT。扩展入口是 `extension/js/extension.js`，但大部分逻辑在 `extension/script/` 下的 Lua 脚本中。

### 三进程调试模型

```
VS Code (JS)  ──DAP──>  Frontend (Lua proxy)  ──socket──>  Master (Lua 线程)
                    extension/script/frontend/          extension/script/backend/master/

                   Master  ──bee.channel──>  Worker (Lua 线程，在目标进程内运行)
                                            extension/script/backend/worker/
```

- **Frontend (`frontend/proxy.lua`)**：桥接 DAP 协议（Content-Length 头 + JSON）和 Master。负责 launch/attach 编排、进程管理、WSL 路径转换。
- **Master (`backend/master/mgr.lua`)**：会话生命周期管理，将 DAP 请求路由到 Worker，管理线程状态，捕获 stdout/stderr。
- **Worker (`backend/worker.lua`)**：在独立 Lua VM 中运行。处理断点、单步、调用栈、变量查看、表达式求值。通过命名 `bee.channel` 通道与 Master 通信。

### 调试器与调试目标的隔离

**Worker 和被调试的 Lua VM 是两个完全独立的 Lua 虚拟机。** 调试器的 Lua 脚本运行在 `luadbg`（内嵌的 Lua 解释器变体）中，被调试的目标程序运行在另一个 Lua VM（可能是不同的 Lua 版本）。两者不能直接传递 Lua 对象（function、userdata 等）。

跨 VM 通信通过原生 C++ 模块桥接：

- **`rdebug_debughost.cpp`** — Worker VM 的入口。`luaopen_luadebug` 在目标进程中创建 Worker VM（`luadbg_State`），将目标 VM 的 `lua_State*` 注册到 Worker 的注册表中。
- **`util/refvalue.h`** — 引用值系统。Worker 中的变量（function、table、userdata 等）实际存储为 **refvalue**（tagged union 的 userdata），描述*如何*在目标 VM 中定位该值（如"第 N 帧的第 M 个局部变量"、"table 数组的第 K 个元素"）。通过 `refvalue::eval(v, hL)` 解析引用，将实际值推入目标 VM 的栈上。
- **`rdebug_visitor.cpp`** — 导出给 Worker Lua 的函数表（`luadebug.visitor` 模块）。每个函数通过 `debughost::get(L)` 获取目标 `lua_State*`，然后操作目标 VM。`copy_to_dbg`/`copy_from_dbg` 在两个 VM 之间传递简单类型（nil、bool、number、string、lightuserdata）的值。
- **`rdebug_hookmgr.cpp`** — Hook 管理。直接操作目标 VM 的 hook 机制。

**这意味着**：Worker Lua 脚本中不能对目标 VM 的值直接调用 `string.dump()`、`pairs()` 等 Lua 标准函数。必须通过 `rdebug` 原生模块提供的函数来间接操作。

### 原生 C++ 库 (`src/luadebug/`)

编译为动态库（`luadebug.dll`/`.so`）。Worker 将其加载到目标进程中以 hook Lua VM：
- `rdebug_init.cpp` — DLL 入口，在目标进程中定位 Lua DLL
- `rdebug_hookmgr.cpp` — Hook 管理（单步/断点 hook）
- `rdebug_visitor.cpp` — 栈帧/变量查看
- `rdebug_debughost.cpp` — Worker VM 生命周期管理，双 VM 绑定
- `compat/` — Lua 版本抽象层（5.x 通过 `5x/`，LuaJIT 通过 `jit/`）
- `thunk/` — 各架构的 hook 跳板代码（x86、x64、ARM64，Windows/macOS/Linux）
- `luadbg/` — 内嵌的 Lua 解释器变体（Worker 的运行时）
- `util/refvalue.h` — 引用值系统，跨 VM 值的懒解析

### 启动器 (`src/launcher/`)

一个小型可执行文件，用于启动目标 Lua 进程或附加到运行中的进程，然后注入调试器 DLL。

### 扩展 Lua 脚本 (`extension/script/`)

| 目录 | 用途 |
|---|---|
| `frontend/` | VS Code DAP 代理、进程创建、平台检测 |
| `backend/master/` | 请求路由、线程管理、事件处理 |
| `backend/worker/` | 断点、表达式求值、变量、序列化、源码映射 |
| `common/` | 共享：协议组帧、JSON、IPC、网络、base64 |

## 关键文件

- `make.lua` — 根构建入口
- `extension/package.json` — VS Code 扩展清单
- `extension/script/debugger.lua` — 调试器入口，由目标进程通过 `dofile` 加载
- `extension/script/launch.lua` / `attach.lua` — 注入目标进程的启动脚本
- `extension/script/backend/bootstrap.lua` — 创建 Master 线程，然后在当前线程初始化 Worker
- `extension/script/common/protocol.lua` — DAP 线协议（Content-Length 头 + JSON）
- `extension/script/backend/worker/hookmgr.lua` — Lua hook 管理器，以 `luadebug.hookmgr` 加载
- `compile/common/config.lua` — 默认构建配置（C11、C++17、debug 模式）

## 代码风格

- Lua：4 空格缩进，数学运算符两侧加空格，函数调用括号前不加空格，保留尾部表分隔符
- C/C++：clang-format，配置文件在 `.clang-format` 和 `.clang-format-ignore`
- `.editorconfig` 中有完整的 Lua 格式化规则

## Git 子模块

本仓库使用 git submodule。克隆或切换分支后若检测到子模块变更，请始终运行 `git submodule update --init`。
