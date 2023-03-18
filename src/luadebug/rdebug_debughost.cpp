#include "rdebug_debughost.h"

#include <stdlib.h>

#include "luadbg/bee_module.h"
#include "rdebug_lua.h"
#include "rdebug_putenv.h"

#if defined(_WIN32) && !defined(LUADBG_DISABLE)
#    include <bee/platform/win/unicode.h>
#endif

static int DEBUG_HOST   = 0;
static int DEBUG_CLIENT = 0;

int event(luadbg_State* cL, lua_State* hL, const char* name, int start);

namespace luadebug::debughost {
    luadbg_State* get_client(lua_State* L) {
        if (lua::rawgetp(L, LUA_REGISTRYINDEX, &DEBUG_CLIENT) != LUA_TLIGHTUSERDATA) {
            lua_pop(L, 1);
            return 0;
        }
        luadbg_State* clientL = (luadbg_State*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return clientL;
    }

    void set(luadbg_State* L, lua_State* hostL) {
        luadbg_pushlightuserdata(L, hostL);
        luadbg_rawsetp(L, LUADBG_REGISTRYINDEX, &DEBUG_HOST);
    }

    lua_State* get(luadbg_State* L) {
        if (luadbg_rawgetp(L, LUADBG_REGISTRYINDEX, &DEBUG_HOST) != LUA_TLIGHTUSERDATA) {
            luadbg_pushstring(L, "Must call in debug client");
            luadbg_error(L);
            return 0;
        }
        lua_State* hL = (lua_State*)luadbg_touserdata(L, -1);
        luadbg_pop(L, 1);
        return hL;
    }

    static void clear_client(lua_State* L) {
        luadbg_State* clientL = get_client(L);
        lua_pushnil(L);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &DEBUG_CLIENT);
        if (clientL) {
            luadbg_close(clientL);
        }
    }

    static int clear(lua_State* L) {
        luadbg_State* clientL = get_client(L);
        if (clientL) {
            event(clientL, L, "exit", 1);
        }
        clear_client(L);
        return 0;
    }

    static int client_main(luadbg_State* L) {
        lua_State* hostL = (lua_State*)luadbg_touserdata(L, 2);
        set(L, hostL);
        luadbg_pushboolean(L, 1);
        luadbg_setfield(L, LUADBG_REGISTRYINDEX, "LUA_NOENV");
        luadbgL_openlibs(L);
        luadebug::require_all(L);

#if !defined(LUADBG_DISABLE) || LUA_VERSION_NUM >= 504
#    if !defined(LUA_GCGEN)
#        define LUA_GCGEN 10
#    endif
        luadbg_gc(L, LUA_GCGEN, 0, 0);
#endif
        const char* mainscript = (const char*)luadbg_touserdata(L, 1);
        if (luadbgL_loadstring(L, mainscript) != LUA_OK) {
            return luadbg_error(L);
        }
        luadbg_pushvalue(L, 3);
        luadbg_call(L, 1, 0);
        return 0;
    }

    static void push_errmsg(lua_State* L, luadbg_State* cL) {
        if (luadbg_type(cL, -1) != LUA_TSTRING) {
            lua_pushstring(L, "Unknown Error");
        }
        else {
            size_t sz       = 0;
            const char* err = luadbg_tolstring(cL, -1, &sz);
            lua_pushlstring(L, err, sz);
        }
    }

    static int start(lua_State* L) {
        clear_client(L);
        lua_CFunction preprocessor = NULL;
        const char* mainscript     = luaL_checkstring(L, 1);
        if (lua_type(L, 2) == LUA_TFUNCTION) {
            preprocessor = lua_tocfunction(L, 2);
            if (preprocessor == NULL) {
                lua_pushstring(L, "Preprocessor must be a C function");
                return lua_error(L);
            }
            if (lua_getupvalue(L, 2, 1)) {
                lua_pushstring(L, "Preprocessor must be a light C function (no upvalue)");
                return lua_error(L);
            }
        }
        luadbg_State* cL = luadbgL_newstate();
        if (cL == NULL) {
            lua_pushstring(L, "Can't new debug client");
            return lua_error(L);
        }

        lua_pushlightuserdata(L, cL);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &DEBUG_CLIENT);

        luadbg_pushcfunction(cL, client_main);
        luadbg_pushlightuserdata(cL, (void*)mainscript);
        luadbg_pushlightuserdata(cL, (void*)L);
        if (preprocessor) {
            // TODO: convert C function？
            luadbg_pushcfunction(cL, (luadbg_CFunction)preprocessor);
        }
        else {
            luadbg_pushnil(cL);
        }

        if (luadbg_pcall(cL, 3, 0, 0) != LUA_OK) {
            push_errmsg(L, cL);
            clear_client(L);
            return lua_error(L);
        }
        return 0;
    }

    static int event(lua_State* L) {
        luadbg_State* clientL = get_client(L);
        if (!clientL) {
            return 0;
        }
        int ok = event(clientL, L, luaL_checkstring(L, 1), 2);
        if (ok < 0) {
            return 0;
        }
        lua_pushboolean(L, ok);
        return 1;
    }

#if defined(_WIN32) && !defined(LUADBG_DISABLE)
    static bee::zstring_view to_strview(lua_State* L, int idx) {
        size_t len      = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return { buf, len };
    }

    static int a2u(lua_State* L) {
        std::string r = bee::win::a2u(to_strview(L, 1));
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }
#endif

    static int setenv(lua_State* L) {
        const char* name  = luaL_checkstring(L, 1);
        const char* value = luaL_checkstring(L, 2);
#if defined(_WIN32)
        lua_pushfstring(L, "%s=%s", name, value);
        luadebug::putenv(lua_tostring(L, -1));
#else
        ::setenv(name, value, 1);
#endif
        return 0;
    }
    static int luaopen(lua_State* L) {
        luaL_Reg l[] = {
            { "start", start },
            { "clear", clear },
            { "event", event },
            { "setenv", setenv },
#if defined(_WIN32) && !defined(LUADBG_DISABLE)
            { "a2u", a2u },
#endif
            { NULL, NULL },
        };
#if LUA_VERSION_NUM == 501
        luaL_register(L, "luadebug", l);
        lua_newuserdata(L, 0);
#else
        luaL_newlibtable(L, l);
        luaL_setfuncs(L, l, 0);
#endif

        lua_createtable(L, 0, 1);
        lua_pushcfunction(L, clear);
        lua_setfield(L, -2, "__gc");
        lua_setmetatable(L, -2);

#if LUA_VERSION_NUM == 501
        lua_rawseti(L, -2, 0);
#endif
        return 1;
    }

}

LUADEBUG_FUNC
int luaopen_luadebug(lua_State* L) {
    return luadebug::debughost::luaopen(L);
}
