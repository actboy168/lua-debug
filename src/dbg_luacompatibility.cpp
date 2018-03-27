#include "dbg_luacompatibility.h"
#include <Windows.h>

namespace lua { 
	version ver = version::v53;

	version get_version() {
		return ver;
	}

	void check_version(void* m) {
		if (m && GetProcAddress((HMODULE)m, "lua_getiuservalue")) {
			ver = version::v54;
		}
	}

namespace lua54 {
	int(__cdecl* lua_getiuservalue)(lua_State *L, int idx, int n);

	int __cdecl lua_getuservalue(lua_State* L, int idx) {
		return lua_getiuservalue(L, idx, 1);
	}
}}
