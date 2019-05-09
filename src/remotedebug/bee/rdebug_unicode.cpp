#include "../onelua/onelua.h"
#include <lua.hpp>
#include "../onelua/replace.h"
#include <binding/lua_unicode.cpp>

extern "C" 
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_unicode(lua_State* L) {
    return luaopen_bee_unicode(L);
}
