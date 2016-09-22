#include "dbg_variables.h" 
#include "dbg_protocol.h"	 
#include "dbg_format.h"
#include <lua.hpp>
#include <set>

namespace vscode
{
	const int type_root = 0;
	const int type_indexmax = 250;
	const int type_metatable = 251;

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

	static int pos2level(int64_t pos)
	{
		int level = 0;
		for (; pos; pos >>= 8)
		{
			++level;
		}
		return level;
	}

	static std::string get_value(lua_State *L, int idx, bool isvalue) {
		if (luaL_callmeta(L, idx, "__tostring")) {
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
		case LUA_TSTRING:
			if (isvalue) {
				return  format("'%s'", lua_tostring(L, idx));
			}
			else {
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

	static bool set_value(lua_State* L, std::string& value)
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
		long lv = strtol(value.c_str(), &end, 10);
		if (*end == 0)
		{
			value = format("%d", lv);
			lua_pushinteger(L, lv);
			return true;
		}

		if (value.size() >= 3 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
		{
			long lv = strtol(value.c_str() + 2, &end, 16);
			if (*end == 0)
			{
				value = format("0x%x", lv);
				lua_pushinteger(L, lv);
				return true;
			}
		}

		end = "";
		double dv = strtod(value.c_str(), &end);
		if (*end == 0)
		{
			value = format("%f", dv);
			lua_pushnumber(L, dv);
			return true;
		}


		if (value.size() >= 2 && value[0] == '\''&& value[value.size() - 1] == '\'')  {
			lua_pushlstring(L, value.data() + 1, value.size() - 2);
			return true;
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
			if (!set_value(L, value))
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
		if (!set_value(L, value))
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
		}

		for (int64_t p = pos; p; p >>= 8)
		{
			level++;
			if (level > 3)
			{
				lua_pop(L, 1);
				return false;
			}

			bool suc = false;
			int n = int(p & 0xFF);
			if (n <= type_indexmax)
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
			else if (n == type_metatable)
			{
				if (lua_getmetatable(L, -1))
				{
					lua_remove(L, -2);
					suc = true;
				}
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
		if (lua_type(L, idx) != LUA_TTABLE)
			return false;
		if (name == "[metatable]")
			return false;
		idx = lua_absindex(L, idx);
		lua_pushnil(L);
		while (lua_next(L, idx))
		{
			if (name == get_value(L, -2, false))
			{
				lua_pop(L, 1);
				if (!set_value(L, value))
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

	variables::variables(wprotocol& res, lua_State* L, lua_Debug* ar)
		: res(res)
		, L(L)
		, ar(ar)
		, n(0)
		, checkstack(lua_gettop(L))
	{
		res("variables").StartArray();
	}

	variables::~variables()
	{
		res.EndArray();
		assert(lua_gettop(L) == checkstack);
		lua_settop(L, checkstack);
	}

	bool variables::push(const std::string& name, const std::string& value, int64_t reference)
	{
		n++;
		for (auto _ : res.Object())
		{
			res("variablesReference").Int64(reference);
			res("name").String(name);
			res("value").String(value);
		}
		if (n + 1 >= type_indexmax)
		{
			for (auto _ : res.Object())
			{
				res("name").String("...");
				res("value").String("");
			}
			return true;
		}
		return false;
	}

	void variables::push_table(int idx, int level, int64_t pos, var_type type)
	{
		size_t n = 0;
		idx = lua_absindex(L, idx);
		if (lua_getmetatable(L, idx))
		{
			std::string value = get_value(L, -1, true);
			int64_t reference = 0;
			if (pos && (lua_type(L, -1) == LUA_TTABLE))
			{
				reference = pos | ((int64_t)type_metatable << ((2 + level) * 8));
			}
			if (push("[metatable]", value, reference))
			{
				lua_pop(L, 1);
				return;
			}
			lua_pop(L, 1);
		}
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			n++;
			std::string name = get_value(L, -2, false);	 
			if (level == 0) {
				if (type == var_type::global) {
					if (standard.find(name) != standard.end()) {
						lua_pop(L, 1);
						continue;
					}
				}
				else if (type == var_type::standard) {
					if (standard.find(name) == standard.end()) {
						lua_pop(L, 1);
						continue;
					}
				}
			}
			std::string value = get_value(L, -1, true);
			int64_t reference = 0;
			if (pos && (lua_type(L, -1) == LUA_TTABLE))
			{
				reference = pos | ((int64_t)n << ((2 + level) * 8));
			}
			lua_pop(L, 1);
			if (push(name, value, reference))
			{
				lua_pop(L, 1);
				break;
			}
		}
	}

	void variables::push_value(var_type type, int depth, int64_t pos)
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
					std::string value = get_value(L, lua_gettop(L), true);
					int64_t reference = 0;
					if (lua_type(L, lua_gettop(L)) == LUA_TTABLE)
					{
						reference = (int)type | (depth << 8) | (n << 16);
					}
					bool quit = push(name, value, reference);
					lua_pop(L, 1);
					if (quit) break;
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
		if (lua_type(L, -1) == LUA_TTABLE)
		{
			if (level == 3)
				push_table(-1, level, 0, type);
			else
				push_table(-1, level, ref, type);
		}
		lua_pop(L, 1);
	}
}
