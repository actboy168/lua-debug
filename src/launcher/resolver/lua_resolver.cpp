#include <resolver/lua_resolver.h>

#include <gumpp.hpp>
#include <string_view>
#include <stdexcept>

namespace luadebug {
    static int (*_lua_pcall)(intptr_t L, int nargs, int nresults, int errfunc);
    static int _lua_pcallk(intptr_t L, int nargs, int nresults, int errfunc, intptr_t ctx, intptr_t k) {
        return _lua_pcall(L, nargs, nresults, errfunc);
    }
    static int (*_luaL_loadbuffer)(intptr_t L, const char* buff, size_t size, const char* name);
    static int _luaL_loadbufferx(intptr_t L, const char* buff, size_t size, const char* name, const char* mode) {
        return _luaL_loadbuffer(L, buff, size, name);
    }

    static void initluamodule(const lua_resolver* resolver) {
        if (resolver->luamodule) return;
        resolver->luamodule = Gum::Process::find_module_by_name(resolver->module_name.data());
        if (!resolver->luamodule) {
            throw std::runtime_error("Lua module not found: " + std::string(resolver->module_name));
        }
    }

    intptr_t lua_resolver::find_export(std::string_view name) const {
        initluamodule(this);
        return (intptr_t)luamodule->find_export_by_name(name.data());
    }

    intptr_t lua_resolver::find_symbol(std::string_view name) const {
        initluamodule(this);
        return (intptr_t)luamodule->find_symbol_by_name(name.data());
    }

    intptr_t lua_resolver::find(std::string_view name) const {
        initluamodule(this);
        using namespace std::string_view_literals;
        for (auto& finder : { &lua_resolver::find_export, &lua_resolver::find_symbol }) {
            if (auto result = (this->*finder)(name)) {
                return result;
            }
            if (name == "lua_pcallk"sv) {
                _lua_pcall = (decltype(_lua_pcall))(this->*finder)("lua_pcall"sv);
                if (_lua_pcall) {
                    return (intptr_t)_lua_pcallk;
                }
            } else if (name == "luaL_loadbufferx"sv) {
                _luaL_loadbuffer = (decltype(_luaL_loadbuffer))(this->*finder)("luaL_loadbuffer"sv);
                if (_luaL_loadbuffer) {
                    return (intptr_t)_luaL_loadbufferx;
                }
            }
        }
        return 0;
    }
}
