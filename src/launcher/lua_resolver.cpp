#include "lua_resolver.h"
#include "gumpp.hpp"
#include <string_view>

namespace lua_delayload {
    lua_resolver::lua_resolver(const std::string_view& module_name)
        : module_name(module_name)
    { }

    static int (*_lua_pcall)(intptr_t L, int nargs, int nresults, int errfunc);
    static int _lua_pcallk(intptr_t L, int nargs, int nresults, int errfunc, intptr_t ctx, intptr_t k) {
        return _lua_pcall(L,nargs,nresults,errfunc);
    }
    static int (*_luaL_loadbuffer)(intptr_t L, const char *buff, size_t size, const char *name);
    static int _luaL_loadbufferx(intptr_t L, const char *buff, size_t size, const char *name, const char *mode) {
        return _luaL_loadbuffer(L,buff,size,name);
    }

    intptr_t lua_resolver::find_export(const char* name) const {
        return (intptr_t)Gum::Process::module_find_export_by_name(module_name.data(), name);
    }

    intptr_t lua_resolver::find_symbol(const char* name) const {
        return (intptr_t)Gum::Process::module_find_symbol_by_name(module_name.data(), name);
    }

    intptr_t lua_resolver::find(const char* name) const {
        auto n = std::string_view(name);
        auto pos = n.find_first_of('.');
        if (pos != std::string_view::npos){
            name = n.substr(pos).data();
        }
        for (auto& finder : {&lua_resolver::find_export, &lua_resolver::find_symbol}) {
            if (auto result = (this->*finder)(name)) {
                return result;
            }
            if (strcmp(name, "lua_pcallk") == 0) {
                _lua_pcall = (decltype(_lua_pcall))(this->*finder)("lua_pcall");
                if (_lua_pcall) {
                    return (intptr_t)_lua_pcallk;
                }
            }
            else if (strcmp(name, "luaL_loadbufferx") == 0) {
                _luaL_loadbuffer = (decltype(_luaL_loadbuffer))(this->*finder)("luaL_loadbuffer");
                if (_luaL_loadbuffer) {
                    return (intptr_t)_luaL_loadbufferx;
                }
            }
        }
        return 0;
    }
}
