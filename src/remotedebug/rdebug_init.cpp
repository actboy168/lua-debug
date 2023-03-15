#if defined(_MSC_VER)

#include <lua.hpp>
#include "rdebug_delayload.h"

extern "C" __declspec(dllexport) int init(lua_State* L) {
    remotedebug::delayload::caller_is_luadll(_ReturnAddress());
    if (lua_type(L, 1) == LUA_TSTRING) {
        remotedebug::delayload::set_luaapi(*(void**)lua_tostring(L, -1));
    }
    else {
        remotedebug::delayload::set_luaapi(nullptr);
    }
    return 0;
}

#endif
