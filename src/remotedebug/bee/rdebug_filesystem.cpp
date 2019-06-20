#define RLUA_REPLACE
#include "../rlua.h"
#include <binding/lua_filesystem.cpp>

RLUA_FUNC
int luaopen_remotedebug_filesystem(rlua_State* L) {
    return luaopen_bee_filesystem(L);
}
