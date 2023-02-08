#include "attach_args.h"
#include "hook/unknown.hpp"

namespace autoattach {
    bool attach_args::get_symbols(void* p) {
		auto resolver = static_cast<symbol_resolver::interface*>(p);
        SymbolResolver(luaL_loadbuffer);
        SymbolResolver(luaL_loadbufferx);
		if (!(luaL_loadbufferx || luaL_loadbuffer))
            return false;
        SymbolResolverWithCheck(lua_pushlstring);
        SymbolResolverWithCheck(lua_settop);
        SymbolResolver(lua_pcallk);
        SymbolResolver(lua_pcall);
        if (!(lua_pcallk || lua_pcall))
            return false;
        SymbolResolver(lua_tolstring);
        return true;
    }
}