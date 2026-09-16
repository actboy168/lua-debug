-- 复现场景的目标进程：VM 主动加载调试器、监听端口、关闭 autoUpdate，改为手动 update。
--
--   set LUA_DEBUG_ROOT=<repo>/publish
--   set LUA_DEBUG_CORE=<repo>/publish/runtime/win32-x64/lua54/luadebug.dll
--   lua.exe test/repro/target.lua <port>

local root = assert(os.getenv "LUA_DEBUG_ROOT", "LUA_DEBUG_ROOT is not set")
local port = assert(arg[1], "need port")

local dbg = dofile(root .. "/script/debugger.lua")
dbg:start {
    address = "127.0.0.1:" .. port,
}

-- 关闭自动 update：调试器只在主循环手动抛 update 时才会处理 GUI 请求（新的断点/暂停等）
-- 带 REPRO_AUTOUPDATE=1 时不关，走调试器默认的 update hook 路径
if os.getenv "REPRO_AUTOUPDATE" ~= "1" then
    dbg:event("autoUpdate", false)
end

-- 调试控制台里那句代码最终会跑到的协程体（真实文件，source 有效）
local function coroutineTask()
    local acc = 0 --@coroutine-first-line
    for i = 1, 3 do
        acc = acc + i
    end
    return acc
end

-- 调试控制台里执行 make_coroutine()：创建协程并立即执行它
function make_coroutine()
    local co = coroutine.create(coroutineTask)
    local ok, res = coroutine.resume(co)
    return ok, res
end

local frameCount = 0
local function frame()
    frameCount = frameCount + 1 --@bp
end

-- 模拟游戏主循环
while true do
    dbg:event("update")
    frame()
end
