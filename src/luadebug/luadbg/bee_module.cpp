#if defined _WIN32
#    include <winsock2.h>
#endif

#define lua_h
#define luaconf_h
#define lualib_h
#define lauxlib_h
#define _LUALIB_H
#define _LUAJIT_H

#include "luadbg/inc/rlua.hpp"
#include "luadbg/inc/rluaexports.h"
#include "luadbg/inc/rluarename.h"
#include "luadbg/bee_module.h"

#include <binding/file.h>
#include <binding/lua_socket.cpp>
#include <binding/lua_thread.cpp>
#include <binding/lua_filesystem.cpp>

#if defined(_WIN32)
#    include <binding/lua_unicode.cpp>
#endif

extern "C" int luaopen_luadebug_hookmgr(rlua_State* L);
extern "C" int luaopen_luadebug_stdio(rlua_State* L);
extern "C" int luaopen_luadebug_utility(rlua_State* L);
extern "C" int luaopen_luadebug_visitor(rlua_State* L);
extern "C" int luaopen_bee_socket(rlua_State* L);
extern "C" int luaopen_bee_thread(rlua_State* L);
extern "C" int luaopen_bee_filesystem(rlua_State* L);
#if defined(_WIN32)
extern "C" int luaopen_bee_unicode(rlua_State* L);
#endif

static rluaL_Reg cmodule[] = {
    { "luadebug.hookmgr", luaopen_luadebug_hookmgr },
    { "luadebug.stdio", luaopen_luadebug_stdio },
    { "luadebug.utility", luaopen_luadebug_utility },
    { "luadebug.visitor", luaopen_luadebug_visitor },
    { "bee.socket", luaopen_bee_socket },
    { "bee.thread", luaopen_bee_thread },
    { "bee.filesystem", luaopen_bee_filesystem },
#if defined(_WIN32)
    { "bee.unicode", luaopen_bee_unicode },
#endif
    { NULL, NULL },
};

namespace luadebug {
    static void require_cmodule() {
        for (const rluaL_Reg* l = cmodule; l->name != NULL; l++) {
            ::bee::lua::register_module(l->name, l->func);
        }
    }
    static ::bee::lua::callfunc _init(require_cmodule);
    int require_all(rlua_State* L) {
        ::bee::lua::preload_module(L);
        return 0;
    }
}
