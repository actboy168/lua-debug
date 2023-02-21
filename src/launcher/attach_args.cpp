#include "attach_args.h"
#include "hook/unknown.hpp"

namespace autoattach {
    bool attach_args::get_symbols(void* p) {
		auto resolver = static_cast<symbol_resolver::interface*>(p);
        SymbolResolver_luaL(loadbuffer);
        if (!loadbuffer) {
            SymbolResolver_luaL(loadbufferx);
            if (!loadbufferx) {
                LOG("can't find luaL_loadbuffer");
                return false;
            }
        }
        SymbolResolverWithCheck_lua(pushlstring);
        SymbolResolverWithCheck_lua(settop);
        SymbolResolver_lua(pcallk);
        if (!pcallk) {
            SymbolResolver_lua(pcall);
            if (!pcall) {
                LOG("can't find lua_pcall");
                return false;
            }
        }
        SymbolResolver_lua(tolstring);
        return true;
    }
}