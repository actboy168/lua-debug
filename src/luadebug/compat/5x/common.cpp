#include "compat/lua.h"

const void* lua_tocfunction_pointer(lua_State* L, int idx) {
    return (const void*)lua_tocfunction(L, idx);
}
