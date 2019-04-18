#include <binding/lua_filesystem.cpp>

extern "C" 
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_filesystem(lua_State* L) {
    return luaopen_bee_filesystem(L);
}
