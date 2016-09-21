#include "dbg_variables.h" 
#include "dbg_protocol.h"	 
#include "dbg_format.h"
#include <lua.hpp>
#include <set>

namespace vscode
{
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
		if (n + 1 >= 250)
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

	const char* variables::getlocal(var_type type, int n)
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

	void variables::push_table(int idx, int level, int64_t pos, var_type type)
	{
		size_t n = 0;
		idx = lua_absindex(L, idx);
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			n++;
			std::string name = get_value(L, -2, false);
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
		int level = 0;
		int64_t ref = (int)type | (depth << 8) | (pos << 16);
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
				for (int n = 1;; n++)
				{
					const char* name = getlocal(type, n);
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
			else
			{
				const char* name = getlocal(type, pos & 0xFF);
				if (!name)
					return;
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
				return;
			}
			int n = int(p & 0xFF);
			bool suc = true;
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

			if (!suc)
			{
				lua_pop(L, 1);
				return;
			}
		}

		if (lua_type(L, -1) == LUA_TTABLE)
		{
			if (level == 3)
				push_table(-1, level, 0, type);
			else
				push_table(-1, level, ref, pos == 0 ? type : var_type::none);
		}
		lua_pop(L, 1);
	}
}
