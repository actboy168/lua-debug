#define RLUA_REPLACE
#include "../rlua.h"
#include "../rdebug_cmodule.h"
#define BEE_THREAD_MODULE (::remotedebug::get_cmodule())
#include <binding/lua_thread.cpp>

RLUA_FUNC
int luaopen_remotedebug_thread(rlua_State* L) {
    return luaopen_bee_thread(L);
}
