#include "../../3rd/bee.lua/binding/lua_socket.cpp"

extern "C" 
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_socket(lua_State* L) {
    return luaopen_bee_socket(L);
}
