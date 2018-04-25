#if defined(DEBUGGER_BRIDGE)

#include <debugger/bridge/lua.h>
#include <Windows.h>
#include <lobject.h>

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

	const void* lua_getproto(lua_State *L, int idx) {
		if (!lua_isfunction(L, idx) || lua_iscfunction(L, idx))
			return 0;
		const LClosure *c = (const LClosure *)lua_topointer(L, idx);
		return c->p;
	}

namespace lua54 {
	int(__cdecl* lua_getiuservalue)(lua_State *L, int idx, int n);

	int __cdecl lua_getuservalue(lua_State* L, int idx) {
		return lua_getiuservalue(L, idx, 1);
	}
}}
#endif
