#pragma once

#include <stddef.h>
#include <string.h>
#include "common.hpp"

struct lua_State;
namespace autoattach {
    struct attach_args {
        typedef intptr_t lua_KContext;
        typedef intptr_t lua_Integer;

        typedef int (*lua_KFunction)(lua_State *L, int status, lua_KContext ctx);

        typedef int (*luaL_loadbuffer_t)(lua_State *L, const char *buff, size_t sz,
                                         const char *name);

        typedef const char *(*lua_tolstring_t)(lua_State *L, int idx, size_t *len);

        typedef void  (*lua_pushlstring_t)(lua_State *L, const char *s, size_t l);

        typedef void  (*lua_settop_t)(lua_State *L, int idx);

        typedef int   (*lua_pcallk_t)(lua_State *L, int nargs, int nresults, int errfunc,
                                      lua_KContext ctx, lua_KFunction k);

        typedef int   (*lua_pcall_t)(lua_State *L, int nargs, int nresults, int errfunc);

        typedef int (*luaL_loadbufferx_t)(lua_State *L, const char *buff, size_t sz,
                                          const char *name, const char *mode);
										  
#define attach_args_funcs(_, ...)\
                    _(luaL_loadbuffer, __VAR_ARGS__)\
                    _(luaL_loadbufferx, __VAR_ARGS__)\
                    _(lua_tolstring, __VAR_ARGS__)\
                    _(lua_pushlstring, __VAR_ARGS__)\
                    _(lua_settop, __VAR_ARGS__)\
                    _(lua_pcallk, __VAR_ARGS__)\
                    _(lua_pcall, __VAR_ARGS__)
#ifndef FILED_VAR
#define FILED_VAR(name, ...)\
                    name##_t name;
#endif

        attach_args_funcs(FILED_VAR);

        inline void lua_pushstring(lua_State *L, const char *s) const {
            lua_pushlstring(L, s, s == nullptr ? 0 : strlen(s));
        }

        bool get_symbols(void*);

        inline int _lua_pcall(lua_State *L, int nargs, int nresults, int errfunc) const {
            if (lua_pcall) {
                return lua_pcall(L, nargs, nresults, errfunc);
            }
            return lua_pcallk(L, nargs, nresults, errfunc, 0, NULL);
        }

        inline int _luaL_loadbuffer(lua_State *L, const char *buff, size_t sz,
                                    const char *name) const {
            if (luaL_loadbufferx) {
                return luaL_loadbufferx(L, buff, sz, name, NULL);
            }
            return luaL_loadbuffer(L, buff, sz, name);
        }

#undef attach_args_funcs
    };
} // namespace autoattach