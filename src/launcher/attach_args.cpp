#include "attach_args.h"
#include "hook/unknown.hpp"

namespace autoattach {
    bool attach_args::get_symbols(void* p) {
#define str(s) #s
#define SymbolResolver_lua(name) name = (name##_t)resolver->getsymbol(str(lua_##name));
#define SymbolResolverWithCheck_lua(name) SymbolResolver_lua(name); if (!name) return false;
		auto resolver = static_cast<symbol_resolver::interface*>(p);
        SymbolResolver_lua(loadbuffer);
        SymbolResolver_lua(loadbufferx);
		if (!(loadbufferx || loadbuffer))
            return false;
        SymbolResolverWithCheck(pushlstring);
        SymbolResolverWithCheck(settop);
        SymbolResolver_lua(pcallk);
        SymbolResolver_lua(pcall);
        if (!(pcallk || pcall))
            return false;
        SymbolResolver_lua(tolstring);
        return true;
    }
}