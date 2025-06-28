#pragma once

#include <stdint.h>

#include <string>

#include "compat/lua.h"
#ifdef LUAJIT_VERSION
const char* lua_cdatatype(lua_State* L, int idx);
int lua_isinteger(lua_State* L, int idx);
#endif

const void* lua_tocfunction_pointer(lua_State* L, int idx);

typedef uint32_t Instruction;
#ifdef LUAJIT_VERSION
union TValue;
struct GCproto;
using CallInfo = TValue;
using Proto    = GCproto;
#else
struct CallInfo;
struct Proto;
struct LuaOpCode {
    int o;
    int a, b, c, bx, sbx;
#    if LUA_VERSION_NUM > 501
    int ax;
#        if LUA_VERSION_NUM == 504
    int sb, sc, isk;
#        endif
#    endif
    int pc;
    const char* name;
    uint32_t* ins;
    std::string tostring() const;
    int line(Proto* f) const;
};
bool lua_getopcode(Proto* f, lua_Integer pc, LuaOpCode& ret);
#endif

Proto* lua_getproto(lua_State* L, int idx);
CallInfo* lua_getcallinfo(lua_State* L);
Proto* lua_ci2proto(CallInfo* ci);
CallInfo* lua_debug2ci(lua_State* L, const lua_Debug* ar);
int lua_getcurrentpc(lua_State* L, CallInfo* ci);
const Instruction* lua_getsavedpc(lua_State* L, CallInfo* ci);

#ifdef LUAJIT_VERSION
int lua_isluafunc(lua_State* L, lua_Debug* ar);
#endif

int lua_stacklevel(lua_State* L);
lua_State* lua_getmainthread(lua_State* L);
