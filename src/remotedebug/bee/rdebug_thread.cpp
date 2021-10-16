#define RLUA_REPLACE
#include "../rlua.h"
#include "../rdebug_cmodule.h"
#include <binding/lua_thread.cpp>

#if defined(_WIN32)
#include <bee/thread/simplethread_win.cpp>
#else
#include <bee/thread/simplethread_posix.cpp>
#endif


RLUA_FUNC
int luaopen_remotedebug_thread(rlua_State* L) {
    return luaopen_bee_thread(L);
}
