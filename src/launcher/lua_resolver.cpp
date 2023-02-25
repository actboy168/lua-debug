#include "lua_resolver.h"
#include "symbol_resolver/symbol_resolver.h"

namespace lua_delayload {
    lua_resolver::lua_resolver(const RuntimeModule &module)
        : context(autoattach::symbol_resolver::symbol_resolver_factory_create(module))
    { }

    static int (*_lua_pcall)(intptr_t L, int nargs, int nresults, int errfunc);
    static int _lua_pcallk(intptr_t L, int nargs, int nresults, int errfunc, intptr_t ctx, intptr_t k) {
        return _lua_pcall(L,nargs,nresults,errfunc);
    }
    static int (*_luaL_loadbuffer)(intptr_t L, const char *buff, size_t size, const char *name);
    static int _luaL_loadbufferx(intptr_t L, const char *buff, size_t size, const char *name, const char *mode) {
        return _luaL_loadbuffer(L,buff,size,name);
    }

    intptr_t lua_resolver::find(const char* name) {
        if (auto result = (intptr_t)context->getsymbol(name)) {
            return result;
        }
        if (strcmp(name, "lua_pcallk") == 0) {
            _lua_pcall = (int (__cdecl *)(intptr_t,int,int,int))context->getsymbol("lua_pcall");
            if (_lua_pcall) {
                return (intptr_t)_lua_pcallk;
            }
        }
        else if (strcmp(name, "luaL_loadbufferx") == 0) {
            _luaL_loadbuffer = (int (__cdecl *)(intptr_t, const char *, size_t, const char *))context->getsymbol("luaL_loadbuffer");
            if (_luaL_loadbuffer) {
                return (intptr_t)_luaL_loadbufferx;
            }
        }
        fprintf(stderr, "Fatal Error: Can't find lua c function: `%s`.", name);
        exit(1);
        return 0;
    }
}
