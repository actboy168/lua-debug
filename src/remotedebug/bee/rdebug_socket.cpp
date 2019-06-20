#define RLUA_REPLACE
#include "../rlua.h"
#include <binding/lua_socket.cpp>

RLUA_FUNC
int luaopen_remotedebug_socket(rlua_State* L) {
    return luaopen_bee_socket(L);
}
