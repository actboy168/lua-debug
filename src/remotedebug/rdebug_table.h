#pragma once

#include "rlua.h"

#ifdef LUAJIT_VERSION
struct GCtab;
using myTable = GCtab;
#else
using myTable = void;
#endif
namespace remotedebug::table {
    unsigned int array_size(const myTable* t);
    unsigned int hash_size(const myTable* t);
    int          get_kv(lua_State* L, const myTable* tv, unsigned int i);
    int          get_k(lua_State* L, const myTable* t, unsigned int i);
    int          get_k(lua_State* L, int idx, unsigned int i);
    int          get_v(lua_State* L, int idx, unsigned int i);
    int          set_v(lua_State* L, int idx, unsigned int i);
}
