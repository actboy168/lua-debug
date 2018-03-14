#include "lua_compatibility.h"

namespace lua { namespace lua54 {
	int(__cdecl* lua_getiuservalue)(lua_State *L, int idx, int n);

	int __cdecl lua_getuservalue(lua_State* L, int idx) {
		return lua_getiuservalue(L, idx, 1);
	}
}}
