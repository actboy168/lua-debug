#include "util/refvalue.h"

#include <bee/nonstd/unreachable.h>

#include <cstring>

#include "rdebug_lua.h"
#include "rdebug_table.h"

namespace luadebug::refvalue {
    template <typename T>
    int eval(T&, lua_State*, value*);

    template <>
    int eval<FRAME_LOCAL>(FRAME_LOCAL& v, lua_State* cL, value*) {
        lua_Debug ar;
        if (lua_getstack(cL, v.frame, &ar) == 0)
            return LUA_TNONE;
        const char* name = lua_getlocal(cL, &ar, v.n);
        if (name) {
            return lua_type(cL, -1);
        }
        return LUA_TNONE;
    }

    template <>
    int eval<FRAME_FUNC>(FRAME_FUNC& v, lua_State* cL, value*) {
        lua_Debug ar;
        if (lua_getstack(cL, v.frame, &ar) == 0)
            return LUA_TNONE;
        if (lua_getinfo(cL, "f", &ar) == 0)
            return LUA_TNONE;
        return LUA_TFUNCTION;
    }

    template <>
    int eval<GLOBAL>(GLOBAL& v, lua_State* cL, value*) {
#if LUA_VERSION_NUM == 501
        lua_pushvalue(cL, LUA_GLOBALSINDEX);
        return LUA_TTABLE;
#else
        return lua::rawgeti(cL, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#endif
    }

    template <>
    int eval<REGISTRY>(REGISTRY& v, lua_State* cL, value*) {
        switch (v.type) {
        case REGISTRY_TYPE::REGISTRY:
            lua_pushvalue(cL, LUA_REGISTRYINDEX);
            return LUA_TTABLE;
        case REGISTRY_TYPE::DEBUG_REF:
            return lua::getfield(cL, LUA_REGISTRYINDEX, "__debugger_ref");
        case REGISTRY_TYPE::DEBUG_WATCH:
            return lua::getfield(cL, LUA_REGISTRYINDEX, "__debugger_watch");
        default:
            std::unreachable();
        }
    }

    template <>
    int eval<STACK>(STACK& v, lua_State* cL, value*) {
        lua_pushvalue(cL, v.index);
        return lua_type(cL, -1);
    }

    template <>
    int eval<UPVALUE>(UPVALUE& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return LUA_TNONE;
        if (t != LUA_TFUNCTION) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        if (lua_getupvalue(cL, -1, v.n)) {
            lua_replace(cL, -2);
            return lua_type(cL, -1);
        }
        else {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
    }

    template <>
    int eval<METATABLE>(METATABLE& v, lua_State* cL, value* parent) {
        switch (v.type) {
        case LUA_TNIL:
            lua_pushnil(cL);
            break;
        case LUA_TBOOLEAN:
            lua_pushboolean(cL, 0);
            break;
        case LUA_TNUMBER:
            lua_pushinteger(cL, 0);
            break;
        case LUA_TSTRING:
            lua_pushstring(cL, "");
            break;
        case LUA_TLIGHTUSERDATA:
            lua_pushlightuserdata(cL, NULL);
            break;
        case LUA_TTABLE:
        case LUA_TUSERDATA: {
            int t = eval(parent, cL);
            if (t == LUA_TNONE)
                return LUA_TNONE;
            if (t != LUA_TTABLE && t != LUA_TUSERDATA) {
                lua_pop(cL, 1);
                return LUA_TNONE;
            }
            break;
        }
        default:
            return LUA_TNONE;
        }
        if (lua_getmetatable(cL, -1)) {
            lua_replace(cL, -2);
            return lua_type(cL, -1);
        }
        else {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
    }

    template <>
    int eval<USERVALUE>(USERVALUE& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return LUA_TNONE;
        if (t != LUA_TUSERDATA) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        t = lua_getiuservalue(cL, -1, v.n);
        lua_replace(cL, -2);
        return t;
    }

    template <>
    int eval<TABLE_ARRAY>(TABLE_ARRAY& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return LUA_TNONE;
        if (t != LUA_TTABLE) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        const void* tv = lua_topointer(cL, -1);
        if (!tv) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        if (!luadebug::table::get_array(cL, tv, v.index)) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        lua_replace(cL, -2);
        return lua_type(cL, -1);
    }

    template <>
    int eval<TABLE_HASH_KEY>(TABLE_HASH_KEY& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return LUA_TNONE;
        if (t != LUA_TTABLE) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        const void* tv = lua_topointer(cL, -1);
        if (!tv) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        if (!luadebug::table::get_hash_k(cL, tv, v.index)) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        lua_replace(cL, -2);
        return lua_type(cL, -1);
    }

    template <>
    int eval<TABLE_HASH_VAL>(TABLE_HASH_VAL& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return LUA_TNONE;
        if (t != LUA_TTABLE) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        const void* tv = lua_topointer(cL, -1);
        if (!tv) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        if (!luadebug::table::get_hash_v(cL, tv, v.index)) {
            lua_pop(cL, 1);
            return LUA_TNONE;
        }
        lua_replace(cL, -2);
        return lua_type(cL, -1);
    }

    int eval(value* v, lua_State* cL) {
        return std::visit([cL, v](auto&& arg) { return eval(arg, cL, v + 1); }, *v);
    }

    template <typename T>
    bool assign(T&, lua_State*, value*) {
        return false;
    }

    template <>
    bool assign<FRAME_LOCAL>(FRAME_LOCAL& v, lua_State* cL, value*) {
        lua_Debug ar;
        if (lua_getstack(cL, v.frame, &ar) == 0) {
            return false;
        }
        if (lua_setlocal(cL, &ar, v.n) != NULL) {
            return true;
        }
        return false;
    }

    template <>
    bool assign<UPVALUE>(UPVALUE& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t != LUA_TFUNCTION)
            return false;
        lua_insert(cL, -2);
        return lua_setupvalue(cL, -2, v.n) != NULL;
    }

    template <>
    bool assign<METATABLE>(METATABLE& v, lua_State* cL, value* parent) {
        switch (v.type) {
        case LUA_TNIL:
            lua_pushnil(cL);
            break;
        case LUA_TBOOLEAN:
            lua_pushboolean(cL, 0);
            break;
        case LUA_TNUMBER:
            lua_pushinteger(cL, 0);
            break;
        case LUA_TSTRING:
            lua_pushstring(cL, "");
            break;
        case LUA_TLIGHTUSERDATA:
            lua_pushlightuserdata(cL, NULL);
            break;
        case LUA_TTABLE:
        case LUA_TUSERDATA: {
            int t = eval(parent, cL);
            if (t != LUA_TTABLE && t != LUA_TUSERDATA) {
                return false;
            }
            break;
        }
        default:
            return false;
        }

        lua_insert(cL, -2);
        int metattype = lua_type(cL, -1);
        if (metattype != LUA_TNIL && metattype != LUA_TTABLE) {
            return false;
        }
        lua_setmetatable(cL, -2);
        return true;
    }

    template <>
    bool assign<USERVALUE>(USERVALUE& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t != LUA_TUSERDATA)
            return false;
        lua_insert(cL, -2);
        lua_setiuservalue(cL, -2, v.n);
        return true;
    }

    template <>
    bool assign<TABLE_ARRAY>(TABLE_ARRAY& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t != LUA_TTABLE)
            return false;
        lua_insert(cL, -2);
        const void* tv = lua_topointer(cL, -2);
        if (!tv)
            return false;
        return luadebug::table::set_array(cL, tv, v.index);
    }

    template <>
    bool assign<TABLE_HASH_VAL>(TABLE_HASH_VAL& v, lua_State* cL, value* parent) {
        int t = eval(parent, cL);
        if (t == LUA_TNONE)
            return false;
        if (t != LUA_TTABLE)
            return false;
        lua_insert(cL, -2);
        const void* tv = lua_topointer(cL, -2);
        if (!tv)
            return false;
        return luadebug::table::set_hash_v(cL, tv, v.index);
    }

    bool assign(value* v, lua_State* cL) {
        int top = lua_gettop(cL);
        bool ok = std::visit([cL, v](auto&& arg) { return assign(arg, cL, v + 1); }, *v);
        lua_settop(cL, top - 1);
        return ok;
    }

    value* create_userdata(luadbg_State* L, int n) {
        return (value*)luadbg_newuserdatauv(L, n * sizeof(value), 0);
    }

    value* create_userdata(luadbg_State* L, int n, int parent) {
        void* parent_data  = luadbg_touserdata(L, parent);
        size_t parent_size = static_cast<size_t>(luadbg_rawlen(L, parent));
        value* v           = (value*)luadbg_newuserdatauv(L, n * sizeof(value) + parent_size, 0);
        memcpy((std::byte*)(v + n), parent_data, parent_size);
        return v;
    }
}
