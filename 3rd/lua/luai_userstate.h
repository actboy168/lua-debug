#include "lua.h"

#if LUA_VERSION_NUM >= 504
#define LUA_CALLHOOK(L,event) luaD_hook(L, event, -1, 0, 0)
#elif LUA_VERSION_NUM >= 502
#define LUA_CALLHOOK(L,event) luaD_hook(L, event, -1)
#elif LUA_VERSION_NUM >= 501
#define LUA_CALLHOOK(L,event) luaD_callhook(L, event, -1)
#else
#error unknown lua version
#endif

#if defined(luai_userstateresume)
#undef luai_userstateresume
#endif

#if defined(luai_userstateyield)
#undef luai_userstateyield
#endif

#define luai_userstateresume(L,n) \
    if (L->hookmask & LUA_MASKTHREAD) LUA_CALLHOOK(L, LUA_HOOKTHREAD);
#define luai_userstateyield(L,n) \
    if (L->hookmask & LUA_MASKTHREAD) LUA_CALLHOOK(L, LUA_HOOKTHREAD);
