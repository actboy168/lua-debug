#define RLUA_REPLACE
#include "../rlua.h"
#include <binding/lua_thread.cpp>

extern "C" 
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_thread(rlua_State* L) {
    return luaopen_bee_thread(L);
}
