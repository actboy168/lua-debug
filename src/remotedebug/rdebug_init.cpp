#if defined(_MSC_VER)

#include <lua.hpp>
#include "rdebug_delayload.h"

extern "C"
__declspec(dllexport)
int init(lua_State *L) {
	remotedebug::delayload::caller_is_luadll(_ReturnAddress());
	void* luaapi = nullptr;
	if (lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		luaapi = lua_touserdata(L, 1)
	}else{
		luaapi = (void*)lua_tostring(L, -1);
	}
	remotedebug::delayload::set_luaapi(luaapi);
	return 0;
}

#endif
