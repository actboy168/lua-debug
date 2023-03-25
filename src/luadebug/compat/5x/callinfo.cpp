#include <lstate.h>

#include "compat/internal.h"

#if LUA_VERSION_NUM < 504
#    define s2v(o) (o)
#endif

#if defined(LUA_VERSION_LATEST)
#    define LUA_STKID(s) s.p
#else
#    define LUA_STKID(s) s
#endif

CallInfo* lua_getcallinfo(lua_State* L) {
    return L->ci;
}

Proto* lua_ci2proto(CallInfo* ci) {
    StkId func = LUA_STKID(ci->func);
#if LUA_VERSION_NUM >= 502
    if (!ttisLclosure(s2v(func))) {
        return 0;
    }
    return clLvalue(s2v(func))->p;
#else
    if (clvalue(func)->c.isC) {
        return 0;
    }
    return clvalue(func)->l.p;
#endif
}

CallInfo* lua_debug2ci(lua_State* L, lua_Debug* ar) {
#if LUA_VERSION_NUM >= 502
    return ar->i_ci;
#else
    return L->base_ci + ar->i_ci;
#endif
}
