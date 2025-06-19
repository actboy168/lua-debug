#if defined _WIN32
#    include <winsock2.h>
#endif

#include "luadbg/lua.hpp"

#if !defined(LUADBG_DISABLE)

#    include "luadbg/inc/luadbgexports.h"
#    include "luadbg/inc/luadbgrename.h"

#    define lua_h
#    define luaconf_h
#    define lualib_h
#    define lauxlib_h
#    define _LUALIB_H
#    define _LUAJIT_H

#endif

#include <bee/lua/module.h>

extern "C" {
#include <3rd/lua-patch/bee_newstate.c>
}
#include <3rd/lua-seri/lua-seri.cpp>
#include <bee/lua/error.cpp>
#include <bee/lua/file.cpp>
#include <bee/lua/luaref.cpp>
#include <bee/version.cpp>
#include <binding/lua_channel.cpp>
#include <binding/lua_epoll.cpp>
#include <binding/lua_filesystem.cpp>
#include <binding/lua_platform.cpp>
#include <binding/lua_select.cpp>
#include <binding/lua_socket.cpp>
#include <binding/lua_sys.cpp>
#include <binding/lua_thread.cpp>

#include "luadbg/bee_module.h"

#if defined(_WIN32)
#    include <binding/port/lua_windows.cpp>
#endif

extern "C" int luaopen_luadebug_hookmgr(luadbg_State* L);
extern "C" int luaopen_luadebug_stdio(luadbg_State* L);
extern "C" int luaopen_luadebug_utility(luadbg_State* L);
extern "C" int luaopen_luadebug_visitor(luadbg_State* L);
extern "C" int luaopen_bee_channel(luadbg_State* L);
extern "C" int luaopen_bee_epoll(luadbg_State* L);
extern "C" int luaopen_bee_filesystem(luadbg_State* L);
extern "C" int luaopen_bee_socket(luadbg_State* L);
extern "C" int luaopen_bee_platform(luadbg_State* L);
extern "C" int luaopen_bee_sys(luadbg_State* L);
extern "C" int luaopen_bee_thread(luadbg_State* L);
#if defined(_WIN32)
extern "C" int luaopen_bee_windows(luadbg_State* L);
#endif

static luadbgL_Reg cmodule[] = {
    { "luadebug.hookmgr", luaopen_luadebug_hookmgr },
    { "luadebug.stdio", luaopen_luadebug_stdio },
    { "luadebug.utility", luaopen_luadebug_utility },
    { "luadebug.visitor", luaopen_luadebug_visitor },
    { "bee.channel", luaopen_bee_channel },
    { "bee.epoll", luaopen_bee_epoll },
    { "bee.filesystem", luaopen_bee_filesystem },
    { "bee.socket", luaopen_bee_socket },
    { "bee.platform", luaopen_bee_platform },
    { "bee.sys", luaopen_bee_sys },
    { "bee.thread", luaopen_bee_thread },
#if defined(_WIN32)
    { "bee.windows", luaopen_bee_windows },
#endif
    { NULL, NULL },
};

namespace luadebug {
    static void require_cmodule() {
        for (const luadbgL_Reg* l = cmodule; l->name != NULL; l++) {
            ::bee::lua::register_module(l->name, l->func);
        }
    }
    static ::bee::lua::callfunc _init(require_cmodule);
    int require_all(luadbg_State* L) {
        ::bee::lua::preload_module(L);
        return 0;
    }
}
