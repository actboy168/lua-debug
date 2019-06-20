#define RLUA_REPLACE
#include "../rlua.h"
#include <binding/lua_unicode.cpp>

RLUA_FUNC
int luaopen_remotedebug_unicode(lua_State* L) {
    return luaopen_bee_unicode(L);
}
