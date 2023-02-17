#if defined(_MSC_VER)

#include <lua.hpp>
#include "rdebug_delayload.h"

extern "C"
__declspec(dllexport)
int init(lua_State *L) {
	remotedebug::delayload::caller_is_luadll(_ReturnAddress());
	remotedebug::delayload::set_luaapi(*(void**)lua_tostring(L, -1));
	return 0;
}

#endif
