#pragma once

#include <stddef.h>
#include <string.h>
#include "common.hpp"

namespace autoattach {
    struct attach_args {
        typedef intptr_t KContext;
        typedef intptr_t lua_Integer;

        typedef int (*KFunction_t)(state_t *L, int status, KContext ctx);

        typedef int (*loadbuffer_t)(state_t *L, const char *buff, size_t sz,
                                         const char *name);

        typedef int (*loadbufferx_t)(state_t *L, const char *buff, size_t sz,
                                          const char *name, const char *mode);
										  
        typedef const char *(*tolstring_t)(state_t *L, int idx, size_t *len);

        typedef void  (*pushlstring_t)(state_t *L, const char *s, size_t l);

        typedef void  (*settop_t)(state_t *L, int idx);

        typedef int   (*pcallk_t)(state_t *L, int nargs, int nresults, int errfunc,
                                      KContext ctx, KFunction_t k);

        typedef int   (*pcall_t)(state_t *L, int nargs, int nresults, int errfunc);

										  
#define attach_args_funcs(_, ...)\
                    _(loadbuffer, __VAR_ARGS__)\
                    _(loadbufferx, __VAR_ARGS__)\
                    _(tolstring, __VAR_ARGS__)\
                    _(pushlstring, __VAR_ARGS__)\
                    _(settop, __VAR_ARGS__)\
                    _(pcallk, __VAR_ARGS__)\
                    _(pcall, __VAR_ARGS__)
#ifndef FILED_VAR
#define FILED_VAR(name, ...)\
                    name##_t name;
#endif

        attach_args_funcs(FILED_VAR);

        inline void pushstring(state_t *L, const char *s) const {
            pushlstring(L, s, s == nullptr ? 0 : strlen(s));
        }

        bool get_symbols(void*);

        inline int _pcall(state_t *L, int nargs, int nresults, int errfunc) const {
            if (pcall) {
                return pcall(L, nargs, nresults, errfunc);
            }
            return pcallk(L, nargs, nresults, errfunc, 0, NULL);
        }

        inline int _loadbuffer(state_t *L, const char *buff, size_t sz,
                                    const char *name) const {
            if (loadbufferx) {
                return loadbufferx(L, buff, sz, name, NULL);
            }
            return loadbuffer(L, buff, sz, name);
        }

        inline void pop(state_t *L, int n) const {
            settop(L, -(n)-1);
        }
#undef attach_args_funcs
    };
} // namespace autoattach