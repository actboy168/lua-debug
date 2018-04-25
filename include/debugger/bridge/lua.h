#pragma once

#if defined(DEBUGGER_BRIDGE)

#include <lua.hpp>

namespace lua {
	enum class version {
		v53,
		v54,
	};
	version get_version();
	void check_version(void* m);

	struct Debug {
		union {
			struct {
				int event;
				const char *name;	/* (n) */
				const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
				const char *what;	/* (S) 'Lua', 'C', 'main', 'tail' */
				const char *source;	/* (S) */
				int currentline;	/* (l) */
				int linedefined;	/* (S) */
				int lastlinedefined;	/* (S) */
				unsigned char nups;	/* (u) number of upvalues */
				unsigned char nparams;/* (u) number of parameters */
				char isvararg;        /* (u) */
				char istailcall;	/* (t) */
				char short_src[LUA_IDSIZE]; /* (S) */
											/* private part */
				struct CallInfo *i_ci;  /* active function */
			};

			struct {
				int event;
				const char *name;	/* (n) */
				const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
				const char *what;	/* (S) 'Lua', 'C', 'main', 'tail' */
				const char *source;	/* (S) */
				int currentline;	/* (l) */
				int linedefined;	/* (S) */
				int lastlinedefined;	/* (S) */
				unsigned char nups;	/* (u) number of upvalues */
				unsigned char nparams;/* (u) number of parameters */
				char isvararg;        /* (u) */
				char istailcall;	/* (t) */
				unsigned short fTransfer;/* (r) index of first value transfered */
				unsigned short nTransfer;   /* (r) number of transfered values */
				char short_src[LUA_IDSIZE]; /* (S) */
											/* private part */
				struct CallInfo *i_ci;  /* active function */
			} v54;
		};
	};

	const void* __cdecl lua_getproto(lua_State *L, int idx);

	namespace lua54 {
		extern int (__cdecl* lua_getiuservalue)(lua_State *L, int idx, int n);
		int __cdecl lua_getuservalue(lua_State* L, int idx);
	}
}

#endif
