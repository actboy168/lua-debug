#include <lua.hpp>
#include <string.h>
#include <stdio.h>

#include "rdebug_debugvar.h"

static int DEBUG_WATCH = 0;
static int DEBUG_WATCH_FUNC = 0;

lua_State* get_host(luaX_State *L);

// frame, index
// return value, name
static int
client_getlocal(luaX_State *L, int getref) {
	int frame = (int)luaXL_checkinteger(L, 1);
	int index = (int)luaXL_checkinteger(L, 2);

	lua_State *hL = get_host(L);

	const char *name = get_frame_local(L, hL, frame, index, getref);
	if (name) {
		luaX_pushstring(L, name);
		luaX_insert(L, -2);
		return 2;
	}

	return 0;
}

static int
lclient_getlocal(luaX_State *L) {
	return client_getlocal(L, 1);
}

static int
lclient_getlocalv(luaX_State *L) {
	return client_getlocal(L, 0);
}

// frame
// return func
static int
lclient_getfunc(luaX_State *L) {
	int frame = (int)luaXL_checkinteger(L, 1);

	lua_State *hL = get_host(L);

	if (get_frame_func(L, hL, frame)) {
		return 1;
	}

	return 0;
}

static int
client_index(luaX_State *L, int getref) {
	lua_State *hL = get_host(L);
	if (luaX_gettop(L) != 2)
		return luaXL_error(L, "need table key");

	if (get_index(L, hL, getref)) {
		return 1;
	}
	return 0;
}

static int
lclient_index(luaX_State *L) {
	return client_index(L, 1);
}

static int
lclient_indexv(luaX_State *L) {
	return client_index(L, 0);
}

static int
client_next(luaX_State *L, int getref) {
	lua_State *hL = get_host(L);
	luaX_settop(L, 2);
	luaX_pushvalue(L, 1);
	// table key table
	luaX_insert(L, -2);
	// table table key
	if (next_key(L, hL, getref) == 0)
		return 0;
	// table key_obj
	luaX_insert(L, 1);
	// key_obj table
	luaX_pushvalue(L, 1);
	// key_obj table key_obj
	if (get_index(L, hL, getref) == 0) {
		return 0;
	}
	return 2;
}

static int
lclient_next(luaX_State *L) {
	return client_next(L, 1);
}

static int
lclient_nextv(luaX_State *L) {
	return client_next(L, 0);
}

static int
client_getstack(luaX_State *L, int getref) {
	int index = (int)luaXL_checkinteger(L, 1);
	lua_State *hL = get_host(L);
	if (get_stack(L, hL, index, getref)) {
		return 1;
	}
	return 0;
}

static int
lclient_getstack(luaX_State *L) {
	return client_getstack(L, 1);
}

static int
lclient_getstackv(luaX_State *L) {
	return client_getstack(L, 0);
}

static int
lclient_copytable(luaX_State *L) {
	lua_State *hL = get_host(L);
	lua_Integer maxn = luaXL_optinteger(L, 2, 0xffff);
	luaX_settop(L, 1);
	if (lua_checkstack(hL, 4) == 0) {
		return luaXL_error(L, "stack overflow");
	}
	if (eval_value(L, hL) != LUA_TTABLE) {
		lua_pop(hL, 1);	// pop table
		return 0;
	}
	luaX_newtable(L);
	luaX_insert(L, -2);
	luaX_pushnil(L);
	// L : result tableref nil
	lua_pushnil(hL);
	// hL : table nil
	while(next_kv(L, hL)) {
		if (--maxn < 0) {
			lua_pop(hL, 2);
			luaX_pop(L, 3);
			return 1;
		}
		// L: result tableref nextkey value
		luaX_pushvalue(L, -2);
		luaX_insert(L, -2);
		// L: result tableref nextkey nextkey value
		luaX_rawset(L, -5);
		// L: result tableref nextkey
	}
	return 1;
}

static int
lclient_value(luaX_State *L) {
	lua_State *hL = get_host(L);
	luaX_settop(L, 1);
	get_value(L, hL);
	return 1;
}

static int
lclient_tostring(luaX_State *L) {
	lua_State *hL = get_host(L);
	luaX_settop(L, 1);
	tostring(L, hL);
	return 1;
}

// userdata ref
// any value
// ref = value
static int
lclient_assign(luaX_State *L) {
	lua_State *hL = get_host(L);
	if (lua_checkstack(hL, 2) == 0)
		return luaXL_error(L, "stack overflow");
	luaX_settop(L, 2);
	int vtype = luaX_type(L, 2);
	switch (vtype) {
	case LUA_TNUMBER:
	case LUA_TNIL:
	case LUA_TBOOLEAN:
	case LUA_TLIGHTUSERDATA:
	case LUA_TSTRING:
		copy_fromX(L, hL);
		break;
	case LUA_TUSERDATA:
		if (eval_value(L, hL) == LUA_TNONE) {
			lua_pushnil(hL);
		}
		break;
	default:
		return luaXL_error(L, "Invalid value type %s", luaX_typename(L, vtype));
	}
	luaXL_checktype(L, 1, LUA_TUSERDATA);
	struct value * ref = (struct value *)luaX_touserdata(L, 1);
	luaX_getuservalue(L, 1);
	int r = assign_value(L, ref, hL);
	luaX_pushboolean(L, r);
	return 1;
}

static int
lclient_type(luaX_State *L) {
	lua_State *hL = get_host(L);

	int t = eval_value(L, hL);
	luaX_pushstring(L, luaX_typename(L, t));
	switch (t) {
	case LUA_TFUNCTION:
		if (lua_iscfunction(hL, -1)) {
			luaX_pushstring(L, "c");
		} else {
			luaX_pushstring(L, "lua");
		}
		break;
	case LUA_TNUMBER:
		if (lua_isinteger(hL, -1)) {
			luaX_pushstring(L, "integer");
		} else {
			luaX_pushstring(L, "float");
		}
		break;
	case LUA_TUSERDATA:
		luaX_pushstring(L, "full");
		break;
	case LUA_TLIGHTUSERDATA:
		luaX_pushstring(L, "light");
		break;
	default:
		lua_pop(hL, 1);
		return 1;
	}
	lua_pop(hL, 1);
	return 2;
}

static int
client_getupvalue(luaX_State *L, int getref) {
	int index = (int)luaXL_checkinteger(L, 2);
	luaX_settop(L, 1);
	lua_State *hL = get_host(L);

	const char *name = get_upvalue(L, hL, index, getref);
	if (name) {
		luaX_pushstring(L, name);
		luaX_insert(L, -2);
		return 2;
	}

	return 0;
}

static int
lclient_getupvalue(luaX_State *L) {
	return client_getupvalue(L, 1);
}

static int
lclient_getupvaluev(luaX_State *L) {
	return client_getupvalue(L, 0);
}

static int
client_getmetatable(luaX_State *L, int getref) {
	luaX_settop(L, 1);
	lua_State *hL = get_host(L);
	if (get_metatable(L, hL, getref)) {
		return 1;
	}
	return 0;
}

static int
lclient_getmetatable(luaX_State *L) {
	return client_getmetatable(L, 1);
}

static int
lclient_getmetatablev(luaX_State *L) {
	return client_getmetatable(L, 0);
}

static int
client_getuservalue(luaX_State *L, int getref) {
	luaX_settop(L, 1);
	lua_State *hL = get_host(L);
	if (get_uservalue(L, hL, getref)) {
		return 1;
	}
	return 0;
}

static int
lclient_getuservalue(luaX_State *L) {
	return client_getuservalue(L, 1);
}

static int
lclient_getuservaluev(luaX_State *L) {
	return client_getuservalue(L, 0);
}

static int
lclient_getinfo(luaX_State *L) {
	luaX_settop(L, 2);
	if (luaX_type(L, 2) != LUA_TTABLE) {
		luaX_pop(L, 1);
		luaX_createtable(L, 0, 7);
	}
	lua_State *hL = get_host(L);
	lua_Debug ar;

	switch (luaX_type(L, 1)) {
	case LUA_TNUMBER:
		if (lua_getstack(hL, (int)luaXL_checkinteger(L, 1), &ar) == 0)
			return 0;
		if (lua_getinfo(hL, "Slnt", &ar) == 0)
			return 0;
		break;
	case LUA_TUSERDATA: {
		luaX_pushvalue(L, 1);
		int t = eval_value(L, hL);
		if (t != LUA_TFUNCTION) {
			if (t != LUA_TNONE) {
				lua_pop(hL, 1);	// remove none function
			}
			return luaXL_error(L, "Need a function ref, It's %s", luaX_typename(L, t));
		}
		luaX_pop(L, 1);
		if (lua_getinfo(hL, ">Slnt", &ar) == 0)
			return 0;
		break;
	}
	default:
		return luaXL_error(L, "Need stack level (integer) or function ref, It's %s", luaX_typename(L, luaX_type(L, 1)));
	}

	luaX_pushstring(L, ar.source);
	luaX_setfield(L, 2, "source");
	luaX_pushstring(L, ar.short_src);
	luaX_setfield(L, 2, "short_src");
	luaX_pushinteger(L, ar.currentline);
	luaX_setfield(L, 2, "currentline");
	luaX_pushinteger(L, ar.linedefined);
	luaX_setfield(L, 2, "linedefined");
	luaX_pushinteger(L, ar.lastlinedefined);
	luaX_setfield(L, 2, "lastlinedefined");
	luaX_pushstring(L, ar.name? ar.name : "?");
	luaX_setfield(L, 2, "name");
	luaX_pushstring(L, ar.what? ar.what : "?");
	luaX_setfield(L, 2, "what");
	if (ar.namewhat) {
		luaX_pushstring(L, ar.namewhat);
	} else {
		luaX_pushnil(L);
	}
	luaX_setfield(L, 2, "namewhat");
	luaX_pushboolean(L, ar.istailcall? 1 : 0);
	luaX_setfield(L, 2, "istailcall");

	return 1;
}

static int
lclient_reffunc(luaX_State *L) {
	size_t len = 0;
	const char* func = luaXL_checklstring(L, 1, &len);
	lua_State* hL = get_host(L);
	if (lua_rawgetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH_FUNC) == LUA_TNIL) {
		lua_pop(hL, 1);
		lua_newtable(hL);
		lua_pushvalue(hL, -1);
		lua_rawsetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH_FUNC);
	}
	if (luaL_loadbuffer(hL, func, len, "=")) {
		luaX_pushnil(L);
		luaX_pushstring(L, lua_tostring(hL, -1));
		lua_pop(hL, 2);
		return 2;
	}
	luaX_pushinteger(L, luaL_ref(hL, -2));
	lua_pop(hL, 1);
	return 1;
}

static int
getreffunc(lua_State *hL, lua_Integer func) {
	if (lua_rawgetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH_FUNC) != LUA_TTABLE) {
		lua_pop(hL, 1);
		return 0;
	}
	if (lua_rawgeti(hL, -1, func) != LUA_TFUNCTION) {
		lua_pop(hL, 2);
		return 0;
	}
	lua_remove(hL, -2);
	return 1;
}

static int
lclient_eval(luaX_State *L) {
	lua_Integer func = luaXL_checkinteger(L, 1);
	const char* source = luaXL_checkstring(L, 2);
	lua_Integer level = luaXL_checkinteger(L, 3);
	lua_State* hL = get_host(L);
	if (!getreffunc(hL, func)) {
		luaX_pushboolean(L, 0);
		luaX_pushstring(L, "invalid func");
		return 2;
	}
	lua_pushstring(hL, source);
	lua_pushinteger(hL, level);
	if (lua_pcall(hL, 2, 1, 0)) {
		luaX_pushboolean(L, 0);
		luaX_pushstring(L, lua_tostring(hL, -1));
		lua_pop(hL, 1);
		return 2;
	}
	luaX_pushboolean(L, 1);
	if (LUA_TNONE == copy_toX(hL, L)) {
		luaX_pushfstring(L, "[%s: %p]", 
			lua_typename(hL, lua_type(hL, -1)),
			lua_topointer(hL, -1)
		);
	}
	lua_pop(hL, 1);
	return 2;
}

static int
addwatch(lua_State *hL, int idx) {
	lua_pushvalue(hL, idx);
	if (lua_rawgetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH) == LUA_TNIL) {
		lua_pop(hL, 1);
		lua_newtable(hL);
		lua_pushvalue(hL, -1);
		lua_rawsetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH);
	}
	lua_insert(hL, -2);
	int ref = luaL_ref(hL, -2);
	lua_pop(hL, 1);
	return ref;
}

static int
storewatch(luaX_State *L, lua_State *hL, int idx) {
	int ref = addwatch(hL, idx);
	lua_remove(hL, idx);
	get_registry(L, VAR_REGISTRY);
	luaX_pushlightuserdata(L, &DEBUG_WATCH);
	if (!get_index(L, hL, 1)) {
		return 0;
	}
	luaX_pushinteger(L, ref);
	if (!get_index(L, hL, 1)) {
		return 0;
	}
	return 1;
}

static int
lclient_evalwatch(luaX_State *L) {
	lua_Integer func = luaXL_checkinteger(L, 1);
	const char* source = luaXL_checkstring(L, 2);
	lua_Integer level = luaXL_checkinteger(L, 3);
	lua_State* hL = get_host(L);
	if (!getreffunc(hL, func)) {
		luaX_pushboolean(L, 0);
		luaX_pushstring(L, "invalid func");
		return 2;
	}
	lua_pushstring(hL, source);
	lua_pushinteger(hL, level);
	int n = lua_gettop(hL) - 3;
	if (lua_pcall(hL, 2, LUA_MULTRET, 0)) {
		luaX_pushboolean(L, 0);
		luaX_pushstring(L, lua_tostring(hL, -1));
		lua_pop(hL, 1);
		return 2;
	}
	int rets = lua_gettop(hL) - n;
	for (int i = 0; i < rets; ++i) {
		if (!storewatch(L, hL, i-rets)) {
			luaX_pushboolean(L, 0);
			luaX_pushstring(L, "error");
			return 2;
		}
	}
	luaX_pushboolean(L, 1);
	luaX_insert(L, -1-rets);
	return 1 + rets;
}

static int
lclient_unwatch(luaX_State *L) {
	lua_Integer ref = luaXL_checkinteger(L, 1);
	lua_State* hL = get_host(L);
	if (lua_rawgetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH) == LUA_TNIL) {
		return 0;
	}
	luaL_unref(hL, -1, (int)ref);
	return 0;
}

static int
lclient_cleanwatch(luaX_State *L) {
	lua_State* hL = get_host(L);
	lua_pushnil(hL);
	lua_rawsetp(hL, LUA_REGISTRYINDEX, &DEBUG_WATCH);
	return 0;
}

int
init_visitor(luaX_State *L) {
	luaXL_Reg l[] = {
		{ "getlocal", lclient_getlocal },
		{ "getlocalv", lclient_getlocalv },
		{ "getfunc", lclient_getfunc },
		{ "getupvalue", lclient_getupvalue },
		{ "getupvaluev", lclient_getupvaluev },
		{ "getmetatable", lclient_getmetatable },
		{ "getmetatablev", lclient_getmetatablev },
		{ "getuservalue", lclient_getuservalue },
		{ "getuservaluev", lclient_getuservaluev },
		//{ "detail", show_detail },
		{ "index", lclient_index },
		{ "indexv", lclient_indexv },
		{ "next", lclient_next },
		{ "nextv", lclient_nextv },
		{ "getstack", lclient_getstack },
		{ "getstackv", lclient_getstackv },
		{ "copytable", lclient_copytable },
		{ "value", lclient_value },
		{ "tostring", lclient_tostring },
		{ "assign", lclient_assign },
		{ "type", lclient_type },
		{ "getinfo", lclient_getinfo },
		{ "reffunc", lclient_reffunc },
		{ "eval", lclient_eval },
		{ "evalwatch", lclient_evalwatch },
		{ "unwatch", lclient_unwatch },
		{ "cleanwatch", lclient_cleanwatch },
		{ NULL, NULL },
	};
	luaX_newtable(L);
	luaXL_setfuncs(L,l,0);
	get_registry(L, VAR_GLOBAL);
	luaX_setfield(L, -2, "_G");
	get_registry(L, VAR_REGISTRY);
	luaX_setfield(L, -2, "_REGISTRY");
	get_registry(L, VAR_MAINTHREAD);
	luaX_setfield(L, -2, "_MAINTHREAD");
	return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_visitor(luaX_State *L) {
	get_host(L);
	return init_visitor(L);
}

lua_State *
getthread(luaX_State *L) {
	luaXL_checktype(L, 1, LUA_TUSERDATA);
	lua_State *hL = get_host(L);
	luaX_pushvalue(L, 1);
	int ct = eval_value(L, hL);
	luaX_pop(L, 1);
	if (ct == LUA_TNONE) {
		luaXL_error(L, "Invalid thread");
		return NULL;
	}
	if (ct != LUA_TTHREAD) {
		lua_pop(hL, 1);
		luaXL_error(L, "Need coroutine, Is %s", lua_typename(hL, ct));
		return NULL;
	}
	lua_State *co = lua_tothread(hL, -1);
	lua_pop(hL, 1);
	return co;
}

int
copyvalue(lua_State *hL, luaX_State *cL) {
	return copy_toX(hL, cL);
}
