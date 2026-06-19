#pragma once

#include "compat/lua.h"

#ifdef LUAJIT_VERSION
const char* lua_cdatatype(lua_State* L, int idx);
int lua_isinteger(lua_State* L, int idx);
#endif

const void* lua_tocfunction_pointer(lua_State* L, int idx);

#ifdef LUAJIT_VERSION
union TValue;
struct GCproto;
union GCobj;
using CallInfo = TValue;
using Proto    = GCproto;
using GCobject = GCobj;
#else
struct CallInfo;
struct Proto;
#endif

Proto* lua_getproto(lua_State* L, int idx);
CallInfo* lua_getcallinfo(lua_State* L);
Proto* lua_ci2proto(CallInfo* ci);
CallInfo* lua_debug2ci(lua_State* L, const lua_Debug* ar);

#ifdef LUAJIT_VERSION
int lua_isluafunc(lua_State* L, lua_Debug* ar);
using lua_foreach_gcobj_cb = void (*)(lua_State* L, void* ud, GCobject* o);
void lua_foreach_gcobj(lua_State* L, lua_foreach_gcobj_cb cb, void* ud);
Proto* lua_toproto(GCobject* o);
bool luajit_set_jitmode(lua_State* L, GCproto* pt, bool enable);
struct ProtoInfo {
    const char* source;
    int linedefined;
    int lastlinedefined;
};
ProtoInfo lua_getprotoinfo(lua_State* L, Proto* pt);
#endif

int lua_stacklevel(lua_State* L);
lua_State* lua_getmainthread(lua_State* L);
