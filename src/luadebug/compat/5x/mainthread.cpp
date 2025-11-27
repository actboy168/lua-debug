#include <lstate.h>

#include "compat/internal.h"

lua_State* lua_getmainthread(lua_State* L) {
#if LUA_VERSION_NUM >= 505
    return mainthread(G(L));
#else
    return L->l_G->mainthread;
#endif
}
