#include "dbg_variables.h" 
#include "dbg_protocol.h"	 
#include "dbg_format.h"
#include <lua.hpp>
#include <set>

namespace vscode
{
	static const int max_level = 4;
	static const int type_root = 0;
	static const int type_indexmax = 250;
	static const int type_metatable = 251;
	static const int type_uservalue = 252;

	static std::set<std::string> standard = {
		"ipairs",
		"error",
		"utf8",
		"rawset",
		"tostring",
		"select",
		"tonumber",
		"_VERSION",
		"loadfile",
		"xpcall",
		"string",
		"rawlen",
		"ravitype",
		"print",
		"rawequal",
		"setmetatable",
		"require",
		"getmetatable",
		"next",
		"package",
		"coroutine",
		"io",
		"_G",
		"math",
		"collectgarbage",
		"os",
		"table",
		"ravi",
		"dofile",
		"pcall",
		"load",
		"module",
		"rawget",
		"debug",
		"assert",
		"type",
		"pairs",
		"bit32",
	};

	static int safe_callmeta(lua_State *L, int obj, const char *event) {
		obj = lua_absindex(L, obj);
		if (luaL_getmetafield(L, obj, event) == LUA_TNIL)
			return 0;
		lua_pushvalue(L, obj);
		if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
			lua_pop(L, 1);
			return 0;
		}
		return 1;
	}

	static int pos2level(int64_t pos)
	{
		int level = 0;
		for (; pos; pos >>= 8)
		{
			++level;
		}
		return level;
	}

	extern intptr_t WATCH_TABLE;
	static bool is_watch_table(lua_State* L, int idx)
	{
		idx = lua_absindex(L, idx);
		if (LUA_TTABLE != lua_rawgetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE)) {
			return false;
		}
		int r = lua_rawequal(L, -1, idx);
		lua_pop(L, 1);
		return !!r;
	}

	bool can_extand(lua_State *L, int idx)
	{
		int type = lua_type(L, idx);
		switch (type)
		{
		case LUA_TTABLE:
			return !is_watch_table(L, idx);
		case LUA_TLIGHTUSERDATA:
			if (lua_getmetatable(L, idx)) {
				lua_pop(L, 1);
				return true;
			}
			lua_pop(L, 1);
			return false;
		case LUA_TUSERDATA:
			if (lua_getmetatable(L, idx)) {
				lua_pop(L, 1);
				return true;
			}
			if (lua_getuservalue(L, idx) != LUA_TNIL) {
				lua_pop(L, 1);
				return true;
			}
			lua_pop(L, 1);
			return false;
		case LUA_TFUNCTION:
			if (lua_getupvalue(L, idx, 1))
			{
				lua_pop(L, 1);
				return true;
			}
			return false;
		}
		return false;
	}

	static bool get_extand_table(lua_State* L, int idx)
	{
		if (!lua_getmetatable(L, idx)) {
			return false;
		}
		int type = lua_getfield(L, -1, "__debugger_extand");
		if (type == LUA_TNIL) {
			lua_pop(L, 2);
			return false;
		}
		if (type == LUA_TTABLE) {
			lua_remove(L, -2);
			return true;
		}
		if (type == LUA_TFUNCTION) {
			lua_pushvalue(L, idx);
			if (lua_pcall(L, 1, 1, 0)) {
				lua_pop(L, 2);
				return false;
			}
			if (lua_type(L, -1) != LUA_TTABLE) {
				lua_pop(L, 2);
				return false;
			}
			lua_remove(L, -2);
			return true;
		}
		lua_pop(L, 2);
		return false;
	}

	static std::string get_name(lua_State *L, int idx) {
		if (safe_callmeta(L, idx, "__tostring")) {
			size_t len = 0;
			const char* buf = lua_tolstring(L, idx, &len);
			std::string result(buf ? buf : "", len);
			lua_pop(L, 1);
			return result;
		}

		switch (lua_type(L, idx)) {
		case LUA_TNUMBER:
			if (lua_isinteger(L, idx))
				return format("%d", lua_tointeger(L, idx));
			else
				return format("%f", lua_tonumber(L, idx));
		case LUA_TSTRING: {
			size_t len = 0;
			const char* buf = lua_tolstring(L, idx, &len);
			return std::string(buf ? buf : "", len);
		}
		case LUA_TBOOLEAN:
			return lua_toboolean(L, idx) ? "true" : "false";
		case LUA_TNIL:
			return "nil";
		default:
			break;
		}
		return format("%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
	}

	static void var_set_value_(variable& var, lua_State *L, int idx, pathconvert& pathconvert)
	{
		switch (lua_type(L, idx)) 
		{
		case LUA_TNUMBER:
			if (lua_isinteger(L, idx))
			{
				var.value = format("%d", lua_tointeger(L, idx)); 
				var.type = "integer";
			}
			else
			{
				var.value = format("%f", lua_tonumber(L, idx));
				var.type = "number";
			}
			return;
		case LUA_TSTRING:
			var.value = format("'%s'", lua_tostring(L, idx));
			var.type = "string";
			return;
		case LUA_TBOOLEAN:
			var.value = lua_toboolean(L, idx) ? "true" : "false";
			var.type = "boolean";
			return;
		case LUA_TNIL:
			var.value = "nil";
			var.type = "nil";
			return;
		case LUA_TFUNCTION:
		{
			if (lua_iscfunction(L, idx))
				break;
			lua_Debug entry;
			lua_pushvalue(L, idx);
			if (lua_getinfo(L, ">S", &entry))
			{
				fs::path client_path;
				custom::result r = pathconvert.eval_uncomplete(entry.source, client_path);
				if (r == custom::result::sucess || r == custom::result::sucess_once)
				{
					var.value = format("[%s:%d]", client_path.string(), entry.linedefined);
					var.type = "function";
					return;
				}
			}
			break;
		}
		default:
			break;
		}
		var.value = format("%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
		var.type = luaL_typename(L, idx);
		return;
	}

	void var_set_value(variable& var, lua_State *L, int idx, pathconvert& pathconvert)
	{
		if (safe_callmeta(L, idx, "__tostring")) {
			var_set_value_(var, L, -1, pathconvert);
			lua_pop(L, 1);
			return;
		}
		return var_set_value_(var, L, idx, pathconvert);
	}

	static bool set_newvalue(lua_State* L, int oldtype, std::string& value)
	{
		if (value == "nil")	{
			lua_pushnil(L);
			return true;
		}
		if (value == "true")	{
			lua_pushboolean(L, 1);
			return true;
		}
		if (value == "false")	{
			lua_pushboolean(L, 0);
			return true;
		}

		char* end = "";
		errno = 0;
		long lv = strtol(value.c_str(), &end, 10);
		if (errno != ERANGE && *end == 0)
		{
			value = format("%d", lv);
			lua_pushinteger(L, lv);
			return true;
		}

		if (value.size() >= 3 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
		{
			errno = 0;
			long lv = strtol(value.c_str() + 2, &end, 16);
			if (errno != ERANGE && *end == 0)
			{
				value = format("0x%x", lv);
				lua_pushinteger(L, lv);
				return true;
			}
		}

		end = "";
		errno = 0;
		double dv = strtod(value.c_str(), &end);
		if (errno != ERANGE && *end == 0)
		{
			value = format("%f", dv);
			lua_pushnumber(L, dv);
			return true;
		}

		if (value.size() >= 2 && value[0] == '\''&& value[value.size() - 1] == '\'')  {
			lua_pushlstring(L, value.data() + 1, value.size() - 2);
			return true;
		}

		if (oldtype != LUA_TSTRING)
		{
			return false;
		}
		lua_pushlstring(L, value.data(), value.size());
		value = "'" + value + "'";
		return true;
	}

	static bool setlocal(lua_State* L, lua_Debug* ar, var_type type, int n, std::string& value)
	{
		if (type == var_type::upvalue)
		{
			if (!lua_getinfo(L, "f", ar))
				return false;
			int oldtype = LUA_TNIL;
			if (lua_getupvalue(L, -1, n))
			{
				oldtype = lua_type(L, -1);
				lua_pop(L, 1);
			}
			if (!set_newvalue(L, oldtype, value))
			{
				lua_pop(L, 1);
				return false;
			}
			if (!lua_setupvalue(L, -2, n)) {
				lua_pop(L, 2);
				return false;
			}
			lua_pop(L, 1);
			return true;
		}

		int oldtype = LUA_TNIL;
		if (lua_setlocal(L, ar, type == var_type::vararg ? -n : n))
		{
			oldtype = lua_type(L, -1);
			lua_pop(L, 1);
		}
		if (!set_newvalue(L, oldtype, value))
			return false;
		if (!lua_setlocal(L, ar, type == var_type::vararg ? -n : n))
		{
			lua_pop(L, 1);
			return false;
		}
		return true;
	}

	static const char* getlocal(lua_State* L, lua_Debug* ar, var_type type, int n)
	{
		if (type == var_type::upvalue)
		{
			if (!lua_getinfo(L, "f", ar))
				return 0;
			const char* r = lua_getupvalue(L, -1, n);
			if (!r) {
				lua_pop(L, 1);
				return 0;
			}
			lua_remove(L, -2);
			return r;
		}
		const char *r = lua_getlocal(L, ar, type == var_type::vararg ? -n : n);
		if (!r) {
			return 0;
		}
		if (*r == '(' && strcmp(r, "(*vararg)") != 0) {
			lua_pop(L, 1);
			return 0;
		}
		if (*r != '(')
			return r;
		static std::string name;
		name = format("[%d]", n);
		return name.c_str();
	}

	bool variables::find_value(lua_State* L, lua_Debug* ar, var_type type, int depth, int64_t pos)
	{
		int level = 0;
		switch (type)
		{
		case var_type::standard:
		case var_type::global:
			lua_pushglobaltable(L);
			break;
		case var_type::local:
		case var_type::vararg:
		case var_type::upvalue:
			if (pos == 0)
			{
				return true;
			}
			else
			{
				const char* name = getlocal(L, ar, type, pos & 0xFF);
				if (!name)
					return false;
				pos >>= 8;
				level++;
			}
			break;
		case var_type::watch:
			pos >>= 8;
			level++;
			break;
		default:
			return false;
		}

		for (int64_t p = pos; p; p >>= 8)
		{
			level++;
			if (level > max_level)
			{
				lua_pop(L, 1);
				return false;
			}

			bool suc = false;
			int n = int(p & 0xFF);
			if (n <= type_indexmax)
			{
				if (LUA_TFUNCTION == lua_type(L, -1))
				{
					if (lua_getupvalue(L, -1, n))
					{
						lua_remove(L, -2);
						suc = true;
					}
				}
				else
				{
					lua_pushnil(L);
					while (lua_next(L, -2))
					{
						n--;
						if (n <= 0)
						{
							lua_remove(L, -2);
							lua_remove(L, -2);
							suc = true;
							break;
						}
						lua_pop(L, 1);
					}
				}
			}
			else if (n == type_metatable)
			{
				if (lua_getmetatable(L, -1))
				{
					lua_remove(L, -2);
					suc = true;
				}
			}
			else if (n == type_uservalue)
			{
				lua_getuservalue(L, -1);
				lua_remove(L, -2);
				suc = true;
			}

			if (!suc)
			{
				lua_pop(L, 1);
				return false;
			}
		}
		return true;
	}

	static bool set_value(lua_State* L, int idx, const std::string& name, std::string& value)
	{
		if (name == "[metatable]")
			return false;
		if (name == "[uservalue]")
			return false;
		idx = lua_absindex(L, idx);
		if (lua_type(L, idx) == LUA_TUSERDATA) {
			if (!get_extand_table(L, idx)) {
				return false;
			}
			if (LUA_TNIL == lua_getfield(L, -1, name.c_str())) {
				lua_pop(L, 1);
				return false;
			}
			int oldtype = lua_type(L, -1);
			lua_pop(L, 1);
			if (!set_newvalue(L, oldtype, value))
			{
				lua_pop(L, 1);
				return false;
			}
			lua_setfield(L, -2, name.c_str());
			return true;
		}
		if (lua_type(L, idx) != LUA_TTABLE)
			return false;
		lua_pushnil(L);
		while (lua_next(L, idx))
		{
			if (name == get_name(L, -2))
			{
				int oldtype = lua_type(L, -1);
				lua_pop(L, 1);
				if (!set_newvalue(L, oldtype, value))
				{
					lua_pop(L, 1);
					return false;
				}
				lua_rawset(L, idx);
				return true;
			}
			lua_pop(L, 1);
		}
		return false;
	}

	bool variables::set_value(lua_State* L, lua_Debug* ar, var_type type, int depth, int64_t pos, const std::string& name, std::string& value)
	{
		switch (type)
		{
		case var_type::local:
		case var_type::vararg:
		case var_type::upvalue:
			if (pos == 0)
			{
				for (int n = 1;; n++)
				{
					const char* lname = getlocal(L, ar, type, n);
					if (!lname)
						break;
					if (name == lname)
					{
						return setlocal(L, ar, type, n, value);
					}
				}
				return false;
			}
			break;
		}

		if (!variables::find_value(L, ar, type, depth, pos))
		{
			return false;
		}
		if (!vscode::set_value(L, -1, name, value))
		{
			lua_pop(L, 1);
			return false;
		}
		lua_pop(L, 1);
		return true;
	}

	variables::variables(wprotocol& res, lua_State* L, lua_Debug* ar, int fix)
		: res(res)
		, L(L)
		, ar(ar)
		, n(0)
		, checkstack(lua_gettop(L) + fix)
	{
		res("variables").StartArray();
	}

	variables::~variables()
	{
		res.EndArray();
		assert(lua_gettop(L) == checkstack);
		lua_settop(L, checkstack);
	}

	bool variables::push(const variable& var)
	{
		n++;
		for (auto _ : res.Object())
		{
			res("variablesReference").Int64(var.reference);
			res("name").String(var.name);
			res("value").String(var.value);
			res("type").String(var.type);
		}
		if (n + 1 >= type_indexmax)
		{
			for (auto _ : res.Object())
			{
				res("variablesReference").Int64(0);
				res("name").String("...");
				res("value").String("");
				res("type").String("");
			}
			return true;
		}
		return false;
	}

	void variables::each_userdata(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert)
	{
		if (!get_extand_table(L, idx)){
			return;
		}

		for (int n = 1;; ++n)
		{
			if (LUA_TSTRING != lua_rawgeti(L, -1, n)) {
				lua_pop(L, 1);
				break;
			}
			lua_pushvalue(L, -1);
			if (LUA_TNIL == lua_gettable(L, -3)) {
				lua_pop(L, 2);
				continue;
			}
			variable var;
			var.name = get_name(L, -2);
			var_set_value(var, L, -1, pathconvert);
			if (pos && can_extand(L, -1))
			{
				var.reference = pos | ((int64_t)n << ((2 + level) * 8));
			}
			if (push(var))
			{
				lua_pop(L, 2);
				break;
			}
			lua_pop(L, 2);
		}
		lua_pop(L, 1);
	}

	void variables::each_function(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert)
	{
		for (int n = 1;; n++)
		{
			const char* name = lua_getupvalue(L, idx, n);
			if (!name)
				break;
			variable var;
			var.name = name;
			var_set_value(var, L, -1, pathconvert);
			if (pos && can_extand(L, lua_gettop(L)))
			{
				var.reference = pos | ((int64_t)n << ((2 + level) * 8));
			}
			lua_pop(L, 1);
			if (push(var)) break;
		}
	}

	void variables::each_table(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert)
	{
		idx = lua_absindex(L, idx);
		if (LUA_TFUNCTION == lua_type(L, idx))
		{
			each_function(idx, level, pos, type, pathconvert);
			return;
		}

		size_t n = 0;
		if (lua_getmetatable(L, idx))
		{
			variable var;
			var.name = "[metatable]";
			var_set_value(var, L, -1, pathconvert);
			if (pos && (lua_type(L, -1) == LUA_TTABLE))
			{
				var.reference = pos | ((int64_t)type_metatable << ((2 + level) * 8));
			}
			if (push(var))
			{
				lua_pop(L, 1);
				return;
			}
			lua_pop(L, 1);
		}

		switch (lua_type(L, -1))
		{
		case LUA_TTABLE:
			break;
		case LUA_TLIGHTUSERDATA:
			return;
		case LUA_TUSERDATA:
			if (lua_getuservalue(L, idx) != LUA_TNIL)
			{
				variable var;
				var.name = "[uservalue]";
				var_set_value(var, L, -1, pathconvert);
				if (pos && can_extand(L, -1))
				{
					var.reference = pos | ((int64_t)type_uservalue << ((2 + level) * 8));
				}
				if (push(var))
				{
					lua_pop(L, 1);
					return;
				}
			}
			lua_pop(L, 1); 
			each_userdata(idx, level, pos, type, pathconvert);
			return;
		}

		std::set<variable> vars;

		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			n++;
			variable var;
			var.name = get_name(L, -2);
			if (level == 0) {
				if (type == var_type::global) {
					if (standard.find(var.name) != standard.end()) {
						lua_pop(L, 1);
						continue;
					}
				}
				else if (type == var_type::standard) {
					if (standard.find(var.name) == standard.end()) {
						lua_pop(L, 1);
						continue;
					}
				}
			}
			var_set_value(var, L, -1, pathconvert);
			if (pos && can_extand(L, -1))
			{
				var.reference = pos | ((int64_t)n << ((2 + level) * 8));
			}
			vars.insert(var);
			lua_pop(L, 1);
		}

		for (const variable& var : vars)
		{
			if (push(var))
			{
				break;
			}
		}
	}

	void variables::push_value(var_type type, int depth, int64_t pos, pathconvert& pathconvert)
	{
		int64_t ref = (int)type | (depth << 8) | (pos << 16);
		switch (type)
		{
		case var_type::local:
		case var_type::vararg:
		case var_type::upvalue:
			if (pos == 0)
			{
				for (int n = 1;; n++)
				{
					const char* name = getlocal(L, ar, type, n);
					if (!name)
						break; 
					variable var;
					var.name = name;
					var_set_value(var, L, -1, pathconvert);
					if (can_extand(L, lua_gettop(L)))
					{
						var.reference = (int)type | (depth << 8) | (n << 16);
					}
					lua_pop(L, 1);
					if (push(var)) break;
				}
				return;
			}
			break;
		}

		if (!find_value(L, ar, type, depth, pos))
		{
			return;
		}

		int level = pos2level(pos);
		if (can_extand(L, -1))
		{
			if (level == max_level)
				each_table(-1, level, 0, type, pathconvert);
			else
				each_table(-1, level, ref, type, pathconvert);
		}
		lua_pop(L, 1);
	}
}
