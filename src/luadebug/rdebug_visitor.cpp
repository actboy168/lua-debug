#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <symbolize/symbolize.h>

#include <algorithm>
#include <limits>

#include "rdebug_debughost.h"
#include "rdebug_lua.h"
#include "rdebug_table.h"
#include "util/protected_area.h"
#include "util/refvalue.h"

using luadebug::protected_area;
using luadebug::protected_call;
namespace refvalue = luadebug::refvalue;

static void checkstack(luadbg_State* L, lua_State* cL, int sz) {
    if (lua_checkstack(cL, sz) == 0) {
        protected_area::raise_error(L, cL, "stack overflow");
    }
}

static int debug_pcall(lua_State* L, int nargs, int nresults, int errfunc) {
#ifdef LUAJIT_VERSION
    global_State* g = G(L);
    bool needClean  = !hook_active(g);
    hook_enter(g);
    int ok = lua_pcall(L, nargs, nresults, errfunc);
    if (needClean)
        hook_leave(g);
#else
    lu_byte oldah = L->allowhook;
    L->allowhook  = 0;
    int ok        = lua_pcall(L, nargs, nresults, errfunc);
    L->allowhook  = oldah;
#endif

    return ok;
}

static int copy_to_dbg(lua_State* from, luadbg_State* to) {
    int t = lua_type(from, -1);
    switch (t) {
    case LUA_TNIL:
        luadbg_pushnil(to);
        break;
    case LUA_TBOOLEAN:
        luadbg_pushboolean(to, lua_toboolean(from, -1));
        break;
    case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503 || defined(LUAJIT_VERSION)
        if (lua_isinteger(from, -1)) {
            luadbg_pushinteger(to, lua_tointeger(from, -1));
        }
        else {
            luadbg_pushnumber(to, lua_tonumber(from, -1));
        }
#else
        luadbg_pushnumber(to, lua_tonumber(from, -1));
#endif
        break;
    case LUA_TSTRING: {
        size_t sz;
        const char* str = lua_tolstring(from, -1, &sz);
        luadbg_pushlstring(to, str, sz);
        break;
    }
    case LUA_TLIGHTUSERDATA:
        luadbg_pushlightuserdata(to, lua_touserdata(from, -1));
        break;
    default:
        return LUA_TNONE;
    }
    return t;
}

static int copy_from_dbg(luadbg_State* from, lua_State* to, int idx) {
    checkstack(from, to, 1);
    int t = luadbg_type(from, idx);
    switch (t) {
    case LUA_TNIL:
        lua_pushnil(to);
        break;
    case LUA_TBOOLEAN:
        lua_pushboolean(to, luadbg_toboolean(from, idx));
        break;
    case LUA_TNUMBER:
        if (luadbg_isinteger(from, idx)) {
            lua_pushinteger(to, (lua_Integer)luadbg_tointeger(from, idx));
        }
        else {
            lua_pushnumber(to, (lua_Number)luadbg_tonumber(from, idx));
        }
        break;
    case LUA_TSTRING: {
        size_t sz;
        const char* str = luadbg_tolstring(from, idx, &sz);
        lua_pushlstring(to, str, sz);
        break;
    }
    case LUA_TLIGHTUSERDATA:
        lua_pushlightuserdata(to, luadbg_touserdata(from, idx));
        break;
    case LUA_TUSERDATA: {
        checkstack(from, to, 3);
        refvalue::value* v = (refvalue::value*)luadbg_touserdata(from, idx);
        return refvalue::eval(v, to);
    }
    default:
        return LUA_TNONE;
    }
    return t;
}

static bool copy_from_dbg(luadbg_State* from, lua_State* to, int idx, int type) {
    int t = copy_from_dbg(from, to, idx);
    if (t == type) {
        return true;
    }
    if (t != LUA_TNONE) {
        lua_pop(to, 1);
    }
    return false;
}

static void eval_copy_args(luadbg_State* from, lua_State* to) {
    if (copy_from_dbg(from, to, -1) == LUA_TNONE) {
        if (luadbg_type(from, -1) == LUA_TTABLE) {
            checkstack(from, to, 3);
            lua_newtable(to);
            luadbg_pushnil(from);
            while (luadbg_next(from, -2)) {
                copy_from_dbg(from, to, -1);
                luadbg_pop(from, 1);
                copy_from_dbg(from, to, -1);
                lua_insert(to, -2);
                lua_rawset(to, -3);
            }
        }
        else {
            lua_pushnil(to);
        }
    }
}

static void registry_table(lua_State* cL, refvalue::REGISTRY_TYPE type) {
    switch (type) {
    case refvalue::REGISTRY_TYPE::REGISTRY:
        lua_pushvalue(cL, LUA_REGISTRYINDEX);
        break;
    case refvalue::REGISTRY_TYPE::DEBUG_REF:
        if (lua::getfield(cL, LUA_REGISTRYINDEX, "__debugger_ref") == LUA_TNIL) {
            lua_pop(cL, 1);
            lua_newtable(cL);
            lua_pushvalue(cL, -1);
            lua_setfield(cL, LUA_REGISTRYINDEX, "__debugger_ref");
        }
        break;
    case refvalue::REGISTRY_TYPE::DEBUG_WATCH:
        if (lua::getfield(cL, LUA_REGISTRYINDEX, "__debugger_watch") == LUA_TNIL) {
            lua_pop(cL, 1);
            lua_newtable(cL);
            lua_pushvalue(cL, -1);
            lua_setfield(cL, LUA_REGISTRYINDEX, "__debugger_watch");
        }
        break;
    default:
        std::unreachable();
    }
}

static int registry_ref(luadbg_State* L, lua_State* cL, refvalue::REGISTRY_TYPE type) {
    int ref = luaL_ref(cL, -2);
    if (ref <= 0) {
        return LUA_NOREF;
    }
    unsigned int index = (unsigned int)(ref - 1);
    const void* tv     = lua_topointer(cL, -1);
    if (!tv || index >= luadebug::table::array_size(tv)) {
        return LUA_NOREF;
    }
    refvalue::create(L, refvalue::TABLE_ARRAY { index }, refvalue::REGISTRY { type });
    return ref;
}

static int registry_ref_simple(luadbg_State* L, lua_State* cL, refvalue::REGISTRY_TYPE type) {
    registry_table(cL, type);
    lua_pushvalue(cL, -2);
    int ref = registry_ref(L, cL, type);
    lua_pop(cL, 1);
    return ref;
}

void registry_unref(lua_State* from, int ref) {
    if (ref >= 0) {
        if (lua::getfield(from, LUA_REGISTRYINDEX, "__debugger_ref") == LUA_TTABLE) {
            luaL_unref(from, -1, ref);
        }
        lua_pop(from, 1);
    }
}

int copy_value(lua_State* from, luadbg_State* to, bool ref) {
    if (copy_to_dbg(from, to) == LUA_TNONE) {
        if (ref) {
            return registry_ref_simple(to, from, refvalue::REGISTRY_TYPE::DEBUG_REF);
        }
        else {
            luadbg_pushfstring(to, "%s: %p", lua_typename(from, lua_type(from, -1)), lua_topointer(from, -1));
            return LUA_NOREF;
        }
    }
    return LUA_NOREF;
}

static int client_getlocal(luadbg_State* L, lua_State* cL, int getref) {
    auto frame = protected_area::checkinteger<uint16_t>(L, 1);
    auto n     = protected_area::checkinteger<int16_t>(L, 2);
    lua_Debug ar;
    if (lua_getstack(cL, frame, &ar) == 0) {
        return 0;
    }
    checkstack(L, cL, 1);
    const char* name = lua_getlocal(cL, &ar, n);
    if (name == NULL)
        return 0;
    if (!getref && copy_to_dbg(cL, L) != LUA_TNONE) {
        lua_pop(cL, 1);
        luadbg_pushstring(L, name);
        luadbg_insert(L, -2);
        return 2;
    }
    lua_pop(cL, 1);
    luadbg_pushstring(L, name);
    refvalue::create(L, refvalue::FRAME_LOCAL { frame, n });
    return 2;
}

static int lclient_getlocal(luadbg_State* L, lua_State* cL) {
    return client_getlocal(L, cL, 1);
}

static int lclient_getlocalv(luadbg_State* L, lua_State* cL) {
    return client_getlocal(L, cL, 0);
}

static int client_field(luadbg_State* L, lua_State* cL, int getref) {
    auto field = protected_area::checkstring(L, 2);
    if (!copy_from_dbg(L, cL, 1, LUA_TTABLE)) {
        return 0;
    }
    checkstack(L, cL, 1);
    lua_pushlstring(cL, field.data(), field.size());
    lua_rawget(cL, -2);
    if (!getref && copy_to_dbg(cL, L) != LUA_TNONE) {
        lua_pop(cL, 2);
        return 1;
    }
    lua_pop(cL, 1);
    const void* tv = lua_topointer(cL, -1);
    if (!tv) {
        lua_pop(cL, 1);
        return 0;
    }
    //
    // 使用简单的O(n)算法查找field，可以更好地保证兼容性。
    // field目前只在很少的场景使用，所以不用在意性能。
    //
    lua_pushlstring(cL, field.data(), field.size());
    lua_insert(cL, -2);
    unsigned int hsize = luadebug::table::hash_size(tv);
    for (unsigned int i = 0; i < hsize; ++i) {
        if (luadebug::table::get_hash_k(cL, tv, i)) {
            if (lua_rawequal(cL, -1, -3)) {
                refvalue::create(L, 1, refvalue::TABLE_HASH_VAL { i });
                lua_pop(cL, 3);
                return 1;
            }
            lua_pop(cL, 1);
        }
    }
    lua_pop(cL, 2);
    return 0;
}

static int lclient_field(luadbg_State* L, lua_State* cL) {
    return client_field(L, cL, 1);
}

static int lclient_fieldv(luadbg_State* L, lua_State* cL) {
    return client_field(L, cL, 0);
}

static int client_tablearray(luadbg_State* L, lua_State* cL, int ref) {
    unsigned int i = protected_area::optinteger<unsigned int>(L, 2, 0);
    unsigned int j = protected_area::optinteger<unsigned int>(L, 3, (std::numeric_limits<unsigned int>::max)());
    luadbg_settop(L, 1);
    checkstack(L, cL, 4);
    if (!copy_from_dbg(L, cL, 1, LUA_TTABLE)) {
        return 0;
    }
    const void* tv = lua_topointer(cL, -1);
    if (!tv) {
        lua_pop(cL, 1);
        return 0;
    }
    luadbg_newtable(L);
    luadbg_Integer n  = 0;
    unsigned int maxn = luadebug::table::array_size(tv);
    if (maxn == 0) {
        lua_pop(cL, 1);
        return 1;
    }
    j = (std::min)(j, maxn - 1);
    for (; i <= j; ++i) {
        bool ok = luadebug::table::get_array(cL, tv, i);
        (void)ok;
        assert(ok);
        if (ref) {
            refvalue::create(L, 1, refvalue::TABLE_ARRAY { i });
            if (copy_to_dbg(cL, L) == LUA_TNONE) {
                luadbg_pushvalue(L, -1);
            }
            luadbg_rawseti(L, -3, ++n);
        }
        else {
            if (copy_to_dbg(cL, L) == LUA_TNONE) {
                refvalue::create(L, 1, refvalue::TABLE_ARRAY { i });
            }
        }
        luadbg_rawseti(L, -2, ++n);
        lua_pop(cL, 1);
    }
    lua_pop(cL, 1);
    return 1;
}

static int lclient_tablearray(luadbg_State* L, lua_State* cL) {
    return client_tablearray(L, cL, 1);
}

static int lclient_tablearrayv(luadbg_State* L, lua_State* cL) {
    return client_tablearray(L, cL, 0);
}

static int client_tablehash(luadbg_State* L, lua_State* cL, int ref) {
    unsigned int i = protected_area::optinteger<unsigned int>(L, 2, 0);
    unsigned int j = protected_area::optinteger<unsigned int>(L, 3, (std::numeric_limits<unsigned int>::max)());
    luadbg_settop(L, 1);
    checkstack(L, cL, 4);
    if (!copy_from_dbg(L, cL, 1, LUA_TTABLE)) {
        return 0;
    }
    const void* tv = lua_topointer(cL, -1);
    if (!tv) {
        lua_pop(cL, 1);
        return 0;
    }
    luadbg_newtable(L);
    luadbg_Integer n  = 0;
    unsigned int maxn = luadebug::table::hash_size(tv);
    if (maxn == 0) {
        lua_pop(cL, 1);
        return 1;
    }
    j = (std::min)(j, maxn - 1);
    for (; i <= j; ++i) {
        if (luadebug::table::get_hash_kv(cL, tv, i)) {
            if (copy_to_dbg(cL, L) == LUA_TNONE) {
                refvalue::create(L, 1, refvalue::TABLE_HASH_KEY { i });
            }
            luadbg_rawseti(L, -2, ++n);
            lua_pop(cL, 1);

            if (ref) {
                refvalue::create(L, 1, refvalue::TABLE_HASH_VAL { i });
                if (copy_to_dbg(cL, L) == LUA_TNONE) {
                    luadbg_pushvalue(L, -1);
                }
                luadbg_rawseti(L, -3, ++n);
            }
            else {
                if (copy_to_dbg(cL, L) == LUA_TNONE) {
                    refvalue::create(L, 1, refvalue::TABLE_HASH_VAL { i });
                }
            }
            luadbg_rawseti(L, -2, ++n);
            lua_pop(cL, 1);
        }
    }
    lua_pop(cL, 1);
    return 1;
}

static int lclient_tablehash(luadbg_State* L, lua_State* cL) {
    return client_tablehash(L, cL, 1);
}

static int lclient_tablehashv(luadbg_State* L, lua_State* cL) {
    return client_tablehash(L, cL, 0);
}

static int lclient_tablesize(luadbg_State* L, lua_State* cL) {
    if (!copy_from_dbg(L, cL, 1, LUA_TTABLE)) {
        return 0;
    }
    const void* t = lua_topointer(cL, -1);
    if (!t) {
        lua_pop(cL, 1);
        return 0;
    }
    luadbg_pushinteger(L, luadebug::table::array_size(t));
    luadbg_pushinteger(L, luadebug::table::hash_size(t));
    lua_pop(cL, 1);
    return 2;
}

static int lclient_udread(luadbg_State* L, lua_State* cL) {
    auto offset = protected_area::checkinteger<luadbg_Integer>(L, 2);
    auto count  = protected_area::checkinteger<luadbg_Integer>(L, 3);
    luadbg_settop(L, 1);
    if (!copy_from_dbg(L, cL, 1, LUA_TUSERDATA)) {
        return 0;
    }
    const char* memory = (const char*)lua_touserdata(cL, -1);
    size_t len         = (size_t)lua_rawlen(cL, -1);
    if (offset < 0 || (size_t)offset >= len || count <= 0) {
        lua_pop(cL, 1);
        return 0;
    }
    if ((size_t)(offset + count) > len) {
        count = (luadbg_Integer)len - offset;
    }
    luadbg_pushlstring(L, memory + offset, (size_t)count);
    lua_pop(cL, 1);
    return 1;
}

static int lclient_udwrite(luadbg_State* L, lua_State* cL) {
    auto offset      = protected_area::checkinteger<luadbg_Integer>(L, 2);
    auto data        = protected_area::checkstring(L, 3);
    int allowPartial = luadbg_toboolean(L, 4);
    luadbg_settop(L, 1);
    if (!copy_from_dbg(L, cL, 1, LUA_TUSERDATA)) {
        return 0;
    }
    const char* memory = (const char*)lua_touserdata(cL, -1);
    size_t len         = (size_t)lua_rawlen(cL, -1);
    if (allowPartial) {
        if (offset < 0 || (size_t)offset >= len) {
            lua_pop(cL, 1);
            luadbg_pushinteger(L, 0);
            return 1;
        }
        size_t bytesWritten = (std::min)(data.size(), (size_t)(len - offset));
        memcpy((void*)(memory + offset), data.data(), bytesWritten);
        lua_pop(cL, 1);
        luadbg_pushinteger(L, bytesWritten);
        return 1;
    }
    else {
        if (offset < 0 || (size_t)offset + data.size() > len) {
            lua_pop(cL, 1);
            return 0;
        }
        memcpy((void*)(memory + offset), data.data(), data.size());
        lua_pop(cL, 1);
        luadbg_pushboolean(L, 1);
        return 1;
    }
}

static int lclient_value(luadbg_State* L, lua_State* cL) {
    luadbg_settop(L, 1);
    if (copy_from_dbg(L, cL, -1) == LUA_TNONE) {
        luadbg_pop(L, 1);
        luadbg_pushnil(L);
        return 1;
    }
    luadbg_pop(L, 1);
    copy_value(cL, L, false);
    lua_pop(cL, 1);
    return 1;
}

// userdata ref
// any value
// ref = value
static int lclient_assign(luadbg_State* L, lua_State* cL) {
    luadbg_settop(L, 2);
    protected_area::check_type(L, 1, LUA_TUSERDATA);
    checkstack(L, cL, 3);
    if (copy_from_dbg(L, cL, -1) == LUA_TNONE) {
        return 0;
    }
    refvalue::value* ref = (refvalue::value*)luadbg_touserdata(L, 1);
    luadbg_pushboolean(L, refvalue::assign(ref, cL));
    return 1;
}

static int lclient_type(luadbg_State* L, lua_State* cL) {
    switch (luadbg_type(L, 1)) {
    case LUA_TNIL:
        luadbg_pushstring(L, "nil");
        return 1;
    case LUA_TBOOLEAN:
        luadbg_pushstring(L, "boolean");
        return 1;
    case LUA_TSTRING:
        luadbg_pushstring(L, "string");
        return 1;
    case LUA_TLIGHTUSERDATA:
        luadbg_pushstring(L, "lightuserdata");
        return 1;
    case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503 || defined(LUAJIT_VERSION)
        if (luadbg_isinteger(L, 1)) {
            luadbg_pushstring(L, "integer");
        }
        else {
            luadbg_pushstring(L, "float");
        }
#else
        luadbg_pushstring(L, "float");
#endif
        return 1;
    case LUA_TUSERDATA:
        break;
    default:
        luadbg_pushstring(L, "unexpected");
        return 1;
    }
    checkstack(L, cL, 3);
    luadbg_settop(L, 1);
    refvalue::value* v = (refvalue::value*)luadbg_touserdata(L, 1);
    int t              = refvalue::eval(v, cL);
    switch (t) {
    case LUA_TNONE:
        luadbg_pushstring(L, "unknown");
        return 1;
    case LUA_TFUNCTION:
        if (lua_iscfunction(cL, -1)) {
            luadbg_pushstring(L, "c function");
        }
        else {
            luadbg_pushstring(L, "function");
        }
        break;
    case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503 || defined(LUAJIT_VERSION)
        if (lua_isinteger(cL, -1)) {
            luadbg_pushstring(L, "integer");
        }
        else {
            luadbg_pushstring(L, "float");
        }
#else
        luadbg_pushstring(L, "float");
#endif
        break;
    case LUA_TLIGHTUSERDATA:
        luadbg_pushstring(L, "lightuserdata");
        break;
#ifdef LUAJIT_VERSION
    case LUA_TCDATA: {
        cTValue* o  = index2adr(cL, -1);
        GCcdata* cd = cdataV(o);
        if (cd->ctypeid == CTID_CTYPEID) {
            luadbg_pushstring(L, "ctype");
        }
        else {
            luadbg_pushstring(L, "cdata");
        }
    } break;
#endif
    default:
        luadbg_pushstring(L, lua_typename(cL, t));
        break;
    }
    lua_pop(cL, 1);
    return 1;
}

static int client_getupvalue(luadbg_State* L, lua_State* cL, int getref) {
    auto index = protected_area::checkinteger<int>(L, 2);
    if (!copy_from_dbg(L, cL, 1, LUA_TFUNCTION)) {
        return 0;
    }
    const char* name = lua_getupvalue(cL, -1, index);
    if (name == NULL) {
        lua_pop(cL, 1);
        return 0;
    }

    if (!getref && copy_to_dbg(cL, L) != LUA_TNONE) {
        lua_pop(cL, 2);
        luadbg_pushstring(L, name);
        luadbg_insert(L, -2);
        return 2;
    }
    lua_pop(cL, 2);
    luadbg_pushstring(L, name);
    refvalue::create(L, -1, refvalue::UPVALUE { index });
    return 2;
}

static int lclient_getupvalue(luadbg_State* L, lua_State* cL) {
    return client_getupvalue(L, cL, 1);
}

static int lclient_getupvaluev(luadbg_State* L, lua_State* cL) {
    return client_getupvalue(L, cL, 0);
}

static int client_getmetatable(luadbg_State* L, lua_State* cL, int getref) {
    luadbg_settop(L, 1);
    checkstack(L, cL, 2);
    int t = copy_from_dbg(L, cL, -1);
    if (t == LUA_TNONE) {
        return 0;
    }
    if (!getref) {
        if (lua_getmetatable(cL, -1) == 0) {
            lua_pop(cL, 1);
            return 0;
        }
        lua_pop(cL, 2);
    }
    else {
        lua_pop(cL, 1);
    }
    if (t == LUA_TTABLE || t == LUA_TUSERDATA) {
        refvalue::create(L, -1, refvalue::METATABLE { t });
        return 1;
    }
    else {
        luadbg_pop(L, 1);
        refvalue::create(L, refvalue::METATABLE { t });
        return 1;
    }
}

static int lclient_getmetatable(luadbg_State* L, lua_State* cL) {
    return client_getmetatable(L, cL, 1);
}

static int lclient_getmetatablev(luadbg_State* L, lua_State* cL) {
    return client_getmetatable(L, cL, 0);
}

static int client_getuservalue(luadbg_State* L, lua_State* cL, int getref) {
    int n = protected_area::optinteger<int>(L, 2, 1);
    checkstack(L, cL, 2);
    if (!copy_from_dbg(L, cL, 1, LUA_TUSERDATA)) {
        return 0;
    }
    if (!getref) {
        if (lua_getiuservalue(cL, -1, n) == LUA_TNONE) {
            lua_pop(cL, 1);
            return 0;
        }
        if (copy_to_dbg(cL, L) != LUA_TNONE) {
            lua_pop(cL, 2);
            luadbg_pushboolean(L, 1);
            return 2;
        }
        lua_pop(cL, 1);
    }
    lua_pop(cL, 1);
    refvalue::create(L, 1, refvalue::USERVALUE { n });
    luadbg_pushboolean(L, 1);
    return 2;
}

static int lclient_getuservalue(luadbg_State* L, lua_State* cL) {
    return client_getuservalue(L, cL, 1);
}

static int lclient_getuservaluev(luadbg_State* L, lua_State* cL) {
    return client_getuservalue(L, cL, 0);
}

static int lclient_getinfo(luadbg_State* L, lua_State* cL) {
    luadbg_settop(L, 3);
    auto options = protected_area::checkstring(L, 2);
    bool hasf    = false;
    int frame    = 0;
    int size     = 0;
#ifdef LUAJIT_VERSION
    bool hasSFlag = false;
#endif
    for (const char* what = options.data(); *what; what++) {
        switch (*what) {
        case 'S':
            size += 5;
#ifdef LUAJIT_VERSION
            hasSFlag = true;
#endif
            break;
        case 'l':
            size += 1;
            break;
        case 'n':
            size += 2;
            break;
        case 'f':
            size += 1;
            hasf = true;
            break;
#if LUA_VERSION_NUM >= 502
        case 'u':
            size += 1;
            break;
        case 't':
            size += 1;
            break;
#endif
#if LUA_VERSION_NUM >= 504
        case 'r':
            size += 2;
            break;
#endif
        default:
            return protected_area::raise_error(L, cL, "invalid option");
        }
    }
    if (luadbg_type(L, 3) != LUA_TTABLE) {
        luadbg_pop(L, 1);
        luadbg_createtable(L, 0, size);
    }

    lua_Debug ar;

    switch (luadbg_type(L, 1)) {
    case LUA_TNUMBER:
        frame = protected_area::checkinteger<int>(L, 1);
        if (lua_getstack(cL, frame, &ar) == 0)
            return 0;
        if (lua_getinfo(cL, options.data(), &ar) == 0)
            return 0;
        if (hasf) lua_pop(cL, 1);
        break;
    case LUA_TUSERDATA: {
        luadbg_pushvalue(L, 1);
        int t = copy_from_dbg(L, cL, -1);
        if (t != LUA_TFUNCTION) {
            if (t != LUA_TNONE) {
                lua_pop(cL, 1);
            }
            return protected_area::raise_error(L, cL, "Need a function ref");
        }
        if (hasf) {
            return protected_area::raise_error(L, cL, "invalid option");
        }
        luadbg_pop(L, 1);
        char what[8];
        what[0] = '>';
        strcpy(what + 1, options.data());
        if (lua_getinfo(cL, what, &ar) == 0)
            return 0;
        break;
    }
    default:
        return protected_area::raise_error(L, cL, "Need stack level (integer) or function ref");
    }
#ifdef LUAJIT_VERSION
    if (hasSFlag && strcmp(ar.what, "main") == 0) {
        // carzy bug,luajit is real linedefined in main file,but in lua it's zero
        // maybe fix it is a new bug
        ar.lastlinedefined = 0;
    }
#endif

    for (const char* what = options.data(); *what; what++) {
        switch (*what) {
        case 'S':
#if LUA_VERSION_NUM >= 504
            luadbg_pushlstring(L, ar.source, ar.srclen);
#else
            luadbg_pushstring(L, ar.source);
#endif
            luadbg_setfield(L, 3, "source");
            luadbg_pushstring(L, ar.short_src);
            luadbg_setfield(L, 3, "short_src");
            luadbg_pushinteger(L, ar.linedefined);
            luadbg_setfield(L, 3, "linedefined");
            luadbg_pushinteger(L, ar.lastlinedefined);
            luadbg_setfield(L, 3, "lastlinedefined");
            luadbg_pushstring(L, ar.what ? ar.what : "?");
            luadbg_setfield(L, 3, "what");
            break;
        case 'l':
            luadbg_pushinteger(L, ar.currentline);
            luadbg_setfield(L, 3, "currentline");
            break;
        case 'n':
            luadbg_pushstring(L, ar.name ? ar.name : "?");
            luadbg_setfield(L, 3, "name");
            if (ar.namewhat) {
                luadbg_pushstring(L, ar.namewhat);
            }
            else {
                luadbg_pushnil(L);
            }
            luadbg_setfield(L, 3, "namewhat");
            break;
        case 'f':
            refvalue::create(L, refvalue::FRAME_FUNC { (uint16_t)frame });
            luadbg_setfield(L, 3, "func");
            break;
#if LUA_VERSION_NUM >= 502
        case 'u':
            luadbg_pushinteger(L, ar.nparams);
            luadbg_setfield(L, 3, "nparams");
            break;
        case 't':
            luadbg_pushboolean(L, ar.istailcall ? 1 : 0);
            luadbg_setfield(L, 3, "istailcall");
            break;
#endif
#if LUA_VERSION_NUM >= 504
        case 'r':
            luadbg_pushinteger(L, ar.ftransfer);
            luadbg_setfield(L, 3, "ftransfer");
            luadbg_pushinteger(L, ar.ntransfer);
            luadbg_setfield(L, 3, "ntransfer");
            break;
#endif
        }
    }

    return 1;
}

static int lclient_load(luadbg_State* L, lua_State* cL) {
    auto func = protected_area::checkstring(L, 1);
    if (luaL_loadbuffer(cL, func.data(), func.size(), "=")) {
        luadbg_pushnil(L);
        luadbg_pushstring(L, lua_tostring(cL, -1));
        lua_pop(cL, 2);
        return 2;
    }
    int ref = registry_ref_simple(L, cL, refvalue::REGISTRY_TYPE::DEBUG_REF);
    lua_pop(cL, 1);
    return ref == LUA_NOREF ? 0 : 1;
}

static int lclient_eval(luadbg_State* L, lua_State* cL) {
    int nargs = luadbg_gettop(L);
    checkstack(L, cL, nargs);
    if (!copy_from_dbg(L, cL, 1, LUA_TFUNCTION)) {
        return 0;
    }
    for (int i = 2; i <= nargs; ++i) {
        luadbg_pushvalue(L, i);
        eval_copy_args(L, cL);
        luadbg_pop(L, 1);
    }
    if (debug_pcall(cL, nargs - 1, 1, 0)) {
        luadbg_pushboolean(L, 0);
        luadbg_pushstring(L, lua_tostring(cL, -1));
        lua_pop(cL, 1);
        return 2;
    }
    luadbg_pushboolean(L, 1);
    copy_value(cL, L, false);
    lua_pop(cL, 1);
    return 2;
}

static int lclient_watch(luadbg_State* L, lua_State* cL) {
    int n     = lua_gettop(cL);
    int nargs = luadbg_gettop(L);
    checkstack(L, cL, nargs);
    if (!copy_from_dbg(L, cL, 1, LUA_TFUNCTION)) {
        return 0;
    }
    for (int i = 2; i <= nargs; ++i) {
        luadbg_pushvalue(L, i);
        eval_copy_args(L, cL);
        luadbg_pop(L, 1);
    }
    if (debug_pcall(cL, nargs - 1, LUA_MULTRET, 0)) {
        luadbg_pushboolean(L, 0);
        luadbg_pushstring(L, lua_tostring(cL, -1));
        lua_pop(cL, 1);
        return 2;
    }
    checkstack(L, cL, 3);
    luadbg_pushboolean(L, 1);
    int rets = lua_gettop(cL) - n;
    luadbgL_checkstack(L, rets, NULL);
    registry_table(cL, refvalue::REGISTRY_TYPE::DEBUG_WATCH);
    for (int i = 0; i < rets; ++i) {
        lua_pushvalue(cL, i - rets - 1);
        if (LUA_NOREF == registry_ref(L, cL, refvalue::REGISTRY_TYPE::DEBUG_WATCH)) {
            luadbg_pushnil(L);
        }
    }
    lua_settop(cL, n);
    return 1 + rets;
}

static int lclient_cleanwatch(luadbg_State* L, lua_State* cL) {
    lua_pushnil(cL);
    lua_setfield(cL, LUA_REGISTRYINDEX, "__debugger_watch");
    return 0;
}

static const char* costatus(lua_State* L, lua_State* co) {
    if (L == co) return "running";
    switch (lua_status(co)) {
    case LUA_YIELD:
        return "suspended";
    case LUA_OK: {
        lua_Debug ar;
        if (lua_getstack(co, 0, &ar)) return "normal";
        if (lua_gettop(co) == 0) return "dead";
        return "suspended";
    }
    default:
        return "dead";
    }
}

static int lclient_costatus(luadbg_State* L, lua_State* cL) {
    if (!copy_from_dbg(L, cL, 1, LUA_TTHREAD)) {
        luadbg_pushstring(L, "invalid");
        return 1;
    }
    const char* s = costatus(cL, lua_tothread(cL, -1));
    lua_pop(cL, 1);
    luadbg_pushstring(L, s);
    return 1;
}

static int lclient_gccount(luadbg_State* L, lua_State* cL) {
    int k    = lua_gc(cL, LUA_GCCOUNT, 0);
    int b    = lua_gc(cL, LUA_GCCOUNTB, 0);
    size_t m = ((size_t)k << 10) & (size_t)b;
    luadbg_pushinteger(L, (luadbg_Integer)m);
    return 1;
}

static int lclient_cfunctioninfo(luadbg_State* L, lua_State* cL) {
    if (copy_from_dbg(L, cL, -1) == LUA_TNONE) {
        luadbg_pushnil(L);
        return 1;
    }
#ifdef LUAJIT_VERSION
    cTValue* o = index2adr(cL, -1);
    void* cfn  = nullptr;
    if (tvisfunc(o)) {
        GCfunc* fn = funcV(o);
        cfn        = (void*)(isluafunc(fn) ? NULL : fn->c.f);
    }
    else if (tviscdata(o)) {
        GCcdata* cd  = cdataV(o);
        CTState* cts = ctype_cts(cL);
        if (cd->ctypeid != CTID_CTYPEID) {
            cfn = cdataptr(cd);
            if (cfn) {
                CType* ct = ctype_get(cts, cd->ctypeid);
                if (ctype_isref(ct->info) || ctype_isptr(ct->info)) {
                    cfn = cdata_getptr(cfn, ct->size);
                    ct  = ctype_rawchild(cts, ct);
                }
                if (!ctype_isfunc(ct->info)) {
                    cfn = nullptr;
                }
                else if (cfn) {
                    cfn = cdata_getptr(cfn, ct->size);
                }
            }
        }
    }
#else
    if (lua_type(cL, -1) != LUA_TFUNCTION) {
        lua_pop(cL, 1);
        luadbg_pushnil(L);
        return 1;
    }
    lua_CFunction cfn = lua_tocfunction(cL, -1);
#endif

    lua_pop(cL, 1);

    auto info = luadebug::symbolize((void*)cfn);
    if (!info.has_value()) {
        luadbg_pushnil(L);
        return 1;
    }
    luadbg_pushlstring(L, info->c_str(), info->size());
    return 1;
}

static int init_visitor(luadbg_State* L) {
    luadbgL_Reg l[] = {
        { "getlocal", protected_call<lclient_getlocal> },
        { "getlocalv", protected_call<lclient_getlocalv> },
        { "getupvalue", protected_call<lclient_getupvalue> },
        { "getupvaluev", protected_call<lclient_getupvaluev> },
        { "getmetatable", protected_call<lclient_getmetatable> },
        { "getmetatablev", protected_call<lclient_getmetatablev> },
        { "getuservalue", protected_call<lclient_getuservalue> },
        { "getuservaluev", protected_call<lclient_getuservaluev> },
        { "field", protected_call<lclient_field> },
        { "fieldv", protected_call<lclient_fieldv> },
        { "tablearray", protected_call<lclient_tablearray> },
        { "tablearrayv", protected_call<lclient_tablearrayv> },
        { "tablehash", protected_call<lclient_tablehash> },
        { "tablehashv", protected_call<lclient_tablehashv> },
        { "tablesize", protected_call<lclient_tablesize> },
        { "udread", protected_call<lclient_udread> },
        { "udwrite", protected_call<lclient_udwrite> },
        { "value", protected_call<lclient_value> },
        { "assign", protected_call<lclient_assign> },
        { "type", protected_call<lclient_type> },
        { "getinfo", protected_call<lclient_getinfo> },
        { "load", protected_call<lclient_load> },
        { "eval", protected_call<lclient_eval> },
        { "watch", protected_call<lclient_watch> },
        { "cleanwatch", protected_call<lclient_cleanwatch> },
        { "costatus", protected_call<lclient_costatus> },
        { "gccount", protected_call<lclient_gccount> },
        { "cfunctioninfo", protected_call<lclient_cfunctioninfo> },
        { NULL, NULL },
    };
    luadbg_newtable(L);
    luadbgL_setfuncs(L, l, 0);
    refvalue::create(L, refvalue::GLOBAL {});
    luadbg_setfield(L, -2, "_G");
    refvalue::create(L, refvalue::REGISTRY { refvalue::REGISTRY_TYPE::REGISTRY });
    luadbg_setfield(L, -2, "_REGISTRY");
    return 1;
}

LUADEBUG_FUNC
int luaopen_luadebug_visitor(luadbg_State* L) {
    luadebug::debughost::get(L);
    return init_visitor(L);
}
