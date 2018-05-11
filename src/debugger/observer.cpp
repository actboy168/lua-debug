#include <debugger/observer.h>
#include <debugger/pathconvert.h>
#include <debugger/impl.h>
#include <debugger/evaluate.h>
#include <base/util/format.h>
#include <set>
#include <array>
#include <base/util/string_view.h>

namespace vscode {

	intptr_t WATCH_TABLE;

	static std::set<std::string_view> standard = {
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
	
	struct scope {
		value::Type type;
		std::string name;
		bool        expensive;
	};
	static scope scopes[] = {
		{ value::Type::local, "Locals", false, },
		{ value::Type::vararg, "Var Args", false, },
		{ value::Type::upvalue, "Upvalues", false, },
		{ value::Type::global, "Globals", true, },
		{ value::Type::standard, "Standard", true, },
	};

	template <class T>
	static T lua_tostr(lua_State* L, int idx)
	{
		size_t len = 0;
		const char* str = lua_tolstring(L, idx, &len);
		return T(str, len);
	}

	static bool has_global(lua_State* L, lua::Debug* ar)
	{
		lua_pushglobaltable(L);
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (lua_type(L, -2) == LUA_TSTRING) {
				std::string_view name = lua_tostr<std::string_view>(L, -2);
				if (standard.find(name) != standard.end()) {
					lua_pop(L, 1);
					continue;
				}
			}
			lua_pop(L, 3);
			return true;
		}
		lua_pop(L, 1);
		return false;
	}

	static bool has_upvalue(lua_State* L, lua::Debug* ar)
	{
		if (!lua_getinfo(L, "f", (lua_Debug*)ar)) {
			return false;
		}
		const char* r = lua_getupvalue(L, -1, 1);
		if (!r) {
			lua_pop(L, 1);
			return false;
		}
		lua_pop(L, 2);
		return true;
	}

	static bool has_local(lua_State* L, lua::Debug* ar)
	{
		for (int i = 1; ; ++i) {
			const char* r = lua_getlocal(L, (lua_Debug*)ar, i);
			if (!r) {
				return false;
			}
			lua_pop(L, 1);
			if (strcmp(r, "(*temporary)") != 0) {
				return true;
			}
		}
	}

	static bool has_vararg(lua_State* L, lua::Debug* ar)
	{
		const char* r = lua_getlocal(L, (lua_Debug*)ar, -1);
		if (!r) {
			return false;
		}
		lua_pop(L, 1);
		return true;
	}

	static bool has_scopes(lua_State* L, lua::Debug* ar, value::Type type)
	{
		switch (type)
		{
		case value::Type::local:    return has_local(L, ar);
		case value::Type::vararg:   return has_vararg(L, ar);
		case value::Type::upvalue:  return has_upvalue(L, ar);
		case value::Type::global:   return has_global(L, ar);
		case value::Type::standard: return true;
		}
		return false;
	}

	static bool get_upvalue(lua_State* L, lua::Debug* ar, int n, const char** name = nullptr)
	{
		if (!lua_getinfo(L, "f", (lua_Debug*)ar))
			return false;
		const char* r = lua_getupvalue(L, -1, n);
		if (!r) {
			lua_pop(L, 1);
			if (name) *name = 0;
			return false;
		}
		lua_remove(L, -2);
		if (name) *name = r;
		return true;
	}

	static bool get_local(lua_State* L, lua::Debug* ar, int n, const char** name = nullptr)
	{
		const char *r = lua_getlocal(L, (lua_Debug*)ar, n);
		if (!r) {
			return false;
		}
		if (strcmp(r, "(*temporary)") == 0) {
			if (name) *name = 0;
			return true;
		}
		if (name) *name = r;
		return true;
	}

	static bool get_vararg(lua_State* L, lua::Debug* ar, int n, const char** name = nullptr)
	{
		const char *r = lua_getlocal(L, (lua_Debug*)ar, -n);
		if (!r) {
			return false;
		}
		if (name) {
			static std::string s_name;
			s_name = base::format("[%d]", n);
			*name = s_name.c_str();
		}
		return true;
	}

	static bool push_setvalue(lua_State* L, int oldtype, set_value& setvalue)
	{
		if (setvalue.value == "nil") {
			lua_pushnil(L);
			setvalue.type = "nil";
			return true;
		}
		if (setvalue.value == "true") {
			lua_pushboolean(L, 1);
			setvalue.type = "boolean";
			return true;
		}
		if (setvalue.value == "false") {
			lua_pushboolean(L, 0);
			setvalue.type = "boolean";
			return true;
		}

		char* end = "";
		errno = 0;
		long lv = strtol(setvalue.value.c_str(), &end, 10);
		if (errno != ERANGE && *end == 0)
		{
			setvalue.value = base::format("%d", lv);
			setvalue.type = "integer";
			lua_pushinteger(L, lv);
			return true;
		}

		if (setvalue.value.size() >= 3 && setvalue.value[0] == '0' && (setvalue.value[1] == 'x' || setvalue.value[1] == 'X'))
		{
			errno = 0;
			long lv = strtol(setvalue.value.c_str() + 2, &end, 16);
			if (errno != ERANGE && *end == 0)
			{
				setvalue.value = base::format("0x%x", lv);
				setvalue.type = "integer";
				lua_pushinteger(L, lv);
				return true;
			}
		}

		end = "";
		errno = 0;
		double dv = strtod(setvalue.value.c_str(), &end);
		if (errno != ERANGE && *end == 0)
		{
			setvalue.value = base::format("%f", dv);
			setvalue.type = "number";
			lua_pushnumber(L, dv);
			return true;
		}

		if (setvalue.value.size() >= 2 && setvalue.value[0] == '\''&& setvalue.value[setvalue.value.size() - 1] == '\'') {
			lua_pushlstring(L, setvalue.value.data() + 1, setvalue.value.size() - 2);
			setvalue.type = "string";
			return true;
		}

		if (oldtype != LUA_TSTRING)
		{
			return false;
		}
		lua_pushlstring(L, setvalue.value.data(), setvalue.value.size());
		setvalue.value = "'" + setvalue.value + "'";
		setvalue.type = "string";
		return true;
	}

	static bool userdata_debugger_extand(lua_State* L, int idx)
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

	static void watch_table(lua_State* L)
	{
		if (LUA_TTABLE != lua_rawgetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE)) {
			lua_pop(L, 1);
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_rawsetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE);
		}
	}

	static void watch_table_clear(lua_State* L)
	{
		lua_pushnil(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE);
	}

	static bool get_watch(lua_State* L, lua::Debug* ar, int n)
	{
		watch_table(L);
		lua_rawgeti(L, -1, n);
		lua_remove(L, -2);
		return true;
	}

	struct var {
		int         n;
		std::string name;
		std::string value;
		std::string type;
		bool extand;

		bool operator<(const var& that) const {
			return name < that.name;
		}

		var(lua_State* L, int n, const char* key, int value, pathconvert& pathconvert)
			: n(n)
			, name(key)
			, value(getValue(L, value, pathconvert))
			, type(getType(L, value))
			, extand(canExtand(L, value))
		{ }

		var(lua_State* L, int n, int key, int value, pathconvert& pathconvert)
		: n(n)
		, name(getName(L, key))
		, value(getValue(L, value, pathconvert))
		, type(getType(L, value))
		, extand(canExtand(L, value))
		{ }

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

		static const char* skipline(const char* str, size_t n)
		{
			if (n <= 1) return str;
			size_t lineno = 1;
			for (const char* z = str; *z; ++z)
			{
				if (*z == '\n' || *z == '\r')
				{
					z++;
					if ((*z == '\n' || *z == '\r') && *z != *(z - 1))
						z++;
					lineno++;
					if (lineno >= n)
					{
						return z;
					}
				}
			}
			return nullptr;
		}

		static std::string getName(lua_State *L, int idx) {
			if (safe_callmeta(L, idx, "__debugger_tostring")) {
				size_t len = 0;
				const char* buf = lua_tolstring(L, idx, &len);
				std::string result(buf ? buf : "", len);
				lua_pop(L, 1);
				return result;
			}

			switch (lua_type(L, idx)) {
			case LUA_TNUMBER:
				if (lua_isinteger(L, idx)) {
					lua_Integer i = lua_tointeger(L, idx);
					if (i > 0 && i < 1000) {
						return base::format("[%03d]", i);
					}
					return base::format("[%d]", i);
				}
				else
					return base::format("%f", lua_tonumber(L, idx));
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
			return base::format("%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
		}

		static std::string getType(lua_State *L, int idx)
		{
			if (luaL_getmetafield(L, idx, "__name") != LUA_TNIL) {
				if (lua_type(L, -1) == LUA_TSTRING) {
					std::string type = lua_tostr<std::string>(L, -1);
					lua_pop(L, 1);
					return type;
				}
				lua_pop(L, 1);
			}
			switch (lua_type(L, idx)) {
			case LUA_TNUMBER:
				return lua_isinteger(L, idx) ? "integer" : "number";
			case LUA_TLIGHTUSERDATA:
				return "light userdata";
			}
			return luaL_typename(L, idx);
		}

		static std::string rawGetShortValue(lua_State *L, int idx, int realIdx, pathconvert& pathconvert)
		{
			switch (lua_type(L, idx)) {
			case LUA_TNUMBER:
				if (lua_isinteger(L, idx)) {
					return base::format("%d", lua_tointeger(L, idx));
				}
				return base::format("%f", lua_tonumber(L, idx));
			case LUA_TSTRING: {
				size_t len = 0;
				const char* str = lua_tolstring(L, idx, &len);
				if (len < 16) {
					return base::format("'%s'", str);
				}
				return base::format("'%s...'", std::string(str, 16));
			}
			case LUA_TBOOLEAN:
				return lua_toboolean(L, idx) ? "true" : "false";
			case LUA_TNIL:
				return "nil";
			case LUA_TTABLE:
				if (var::canExtand(L, idx)) {
					return "...";
				}
				return "{}";
			case LUA_TFUNCTION:
				return "func";
			default:
				break;
			}
			return luaL_typename(L, realIdx);
		}

		static std::string getShortValue(lua_State *L, int idx, pathconvert& pathconvert)
		{
			idx = lua_absindex(L, idx);
			if (safe_callmeta(L, idx, "__debugger_tostring")) {
				std::string r = rawGetShortValue(L, -1, idx, pathconvert);
				lua_pop(L, 1);
				return r;
			}
			if (lua_isuserdata(L, idx) && userdata_debugger_extand(L, idx)) {
				std::string r = rawGetShortValue(L, -1, idx, pathconvert);
				lua_pop(L, 1);
				return r;
			}
			return rawGetShortValue(L, idx, idx, pathconvert);
		}

		static std::string getDebuggerExtandValue(lua_State *L, int idx, size_t maxlen, pathconvert& pathconvert)
		{
			std::string s = "";
			for (int n = 1;; ++n) {
				if (LUA_TSTRING != lua_rawgeti(L, idx, n)) {
					lua_pop(L, 1);
					break;
				}
				lua_pushvalue(L, -1);
				if (LUA_TNIL == lua_gettable(L, -3)) {
					lua_pop(L, 2);
					continue;
				}
				std::string name = getName(L, -2);
				std::string value = getShortValue(L, -1, pathconvert);
				s += name + "=" + value + ",";
				lua_pop(L, 2);
				if (s.size() >= maxlen) {
					return base::format("{%s...}", s);
				}
			}
			return base::format("{%s}", s);
		}

		static std::string getTableValue(lua_State *L, int idx, size_t maxlen, pathconvert& pathconvert)
		{
			std::set<int> mark;
			std::string s = "";
			for (int n = 1;; ++n) {
				if (LUA_TNIL == lua_rawgeti(L, idx, n)) {
					lua_pop(L, 1);
					break;
				}
				std::string value = getShortValue(L, -1, pathconvert);
				s += value + ",";
				mark.insert(n);
				lua_pop(L, 1);
				if (s.size() >= maxlen) {
					return base::format("{%s...}", s);
				}
			}

			std::map<std::string, std::string> vars;
			lua_pushnil(L);
			while (lua_next(L, idx)) {
				if (lua_isinteger(L, -2)) {
					int n = (int)lua_tointeger(L, -2);
					if (mark.find(n) != mark.end()) {
						lua_pop(L, 1);
						continue;
					}
				}
				if (vars.size() >= 300) {
					lua_pop(L, 2);
					break;
				}
				vars.insert(std::make_pair(getName(L, -2), getShortValue(L, -1, pathconvert)));
				lua_pop(L, 1);
			}

			for (auto& var : vars) {
				s += var.first + "=" + var.second + ",";
				if (s.size() >= maxlen) {
					return base::format("{%s...}", s);
				}
			}
			return base::format("{%s}", s);
		}

		static std::string rawGetValue(lua_State *L, int idx, int realIdx, size_t maxlen, pathconvert& pathconvert)
		{
			switch (lua_type(L, idx)) {
			case LUA_TNUMBER:
				if (lua_isinteger(L, idx)) {
					return base::format("%d", lua_tointeger(L, idx));
				}
				return base::format("%f", lua_tonumber(L, idx));
			case LUA_TSTRING: {
				size_t len = 0;
				const char* str = lua_tolstring(L, idx, &len);
				if (len < 256) {
					return base::format("'%s'", str);
				}
				return base::format("'%s...'", std::string(str, 256));
			}
			case LUA_TBOOLEAN:
				return lua_toboolean(L, idx) ? "true" : "false";
			case LUA_TNIL:
				return "nil";
			case LUA_TFUNCTION: {
				if (lua_iscfunction(L, idx)) {
					return "C function";
				}
				lua::Debug entry;
				lua_pushvalue(L, idx);
				if (lua_getinfo(L, ">S", (lua_Debug*)&entry)) {
					if (entry.source && entry.source[0] != '@' && entry.source[0] != '=') {
						const char* pos = skipline(entry.source, entry.linedefined);
						if (pos) {
							return pos;
						}
						return base::format("%s:%d", entry.source, entry.linedefined);
					}
					if (entry.source) {
						std::string client_path;
						if (pathconvert.path_convert(entry.source, client_path)) {
							return base::format("%s:%d", pathconvert.path_clientrelative(client_path), entry.linedefined);
						}
					}
				}
				break;
			}
			case LUA_TTABLE:
				return getTableValue(L, idx, maxlen, pathconvert);
			case LUA_TUSERDATA:
				if (luaL_getmetafield(L, idx, "__name") != LUA_TNIL) {
					if (lua_type(L, -1) == LUA_TSTRING) {
						std::string type = lua_tostr<std::string>(L, -1);
						lua_pop(L, 1);
						return base::format("userdata: %s", type);
					}
					lua_pop(L, 1);
				}
				return "userdata";
			default:
				break;
			}
			return luaL_typename(L, realIdx);
		}

		static std::string getValue(lua_State *L, int idx, pathconvert& pathconvert)
		{
			size_t maxlen = 32;
			idx = lua_absindex(L, idx);
			if (safe_callmeta(L, idx, "__debugger_tostring")) {
				std::string r = rawGetValue(L, -1, idx, maxlen, pathconvert);
				lua_pop(L, 1);
				return r;
			}
			if (lua_isuserdata(L, idx) && userdata_debugger_extand(L, idx)) {
				std::string r = getDebuggerExtandValue(L, -1, maxlen, pathconvert);
				lua_pop(L, 1);
				return r;
			}
			return rawGetValue(L, idx, idx, maxlen, pathconvert);
		}

		static bool is_watch_table(lua_State* L, int idx)
		{
			idx = lua_absindex(L, idx);
			if (LUA_TTABLE != lua_rawgetp(L, LUA_REGISTRYINDEX, &WATCH_TABLE)) {
				lua_pop(L, 1);
				return false;
			}
			int r = lua_rawequal(L, -1, idx);
			lua_pop(L, 1);
			return !!r;
		}

		static bool table_empty(lua_State *L, int idx)
		{
			idx = lua_absindex(L, idx);
			lua_pushnil(L);
			if (0 == lua_next(L, idx))
			{
				return true;
			}
			lua_pop(L, 2);
			return false;
		}

		static bool canExtand(lua_State *L, int idx)
		{
			int type = lua_type(L, idx);
			switch (type)
			{
			case LUA_TTABLE:
				return !is_watch_table(L, idx) && !table_empty(L, idx);
			case LUA_TLIGHTUSERDATA:
				if (lua_getmetatable(L, idx)) {
					lua_pop(L, 1);
					return true;
				}
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
	};

	frame::frame(int threadId, int frameId)
		: threadId(threadId)
		, frameId(frameId)
	{ }

	void frame::new_scope(lua_State* L, lua::Debug* ar, wprotocol& res)
	{
		for (auto _ : res("scopes").Array()) 
		{
			for (auto scope : scopes)
			{
				if (has_scopes(L, ar, scope.type))
				{
					for (auto _ : res.Object())
					{
						res("name").String(scope.name);
						res("variablesReference").Int64(new_variable(-1, scope.type, 0));
						res("expensive").Bool(scope.expensive);
					}
				}
			}
		}
	}

	void frame::clear()
	{
		values.clear();
	}

	int64_t frame::new_variable(size_t parent, value::Type type, int index)
	{
		size_t n = values.size();
		value v = {
			n,
			parent,
			type,
			index,
		};
		values.emplace_back(std::move(v));
		return (1 + n) | (frameId << 16) | ((int64_t)threadId << 32);
	}

	bool frame::push_value(lua_State* L, lua::Debug* ar, const value& v)
	{
		switch (v.type) {
		case value::Type::upvalue: return get_upvalue(L, ar, v.index);
		case value::Type::local: return get_local(L, ar, v.index);
		case value::Type::vararg: return get_vararg(L, ar, v.index);
		case value::Type::watch: return get_watch(L, ar, v.index);
		}

		if (v.parent != -1) {
			if (!push_value(L, ar, values[v.parent])) {
				return false;
			}
		}
		switch (v.type) {
		case value::Type::global:
		case value::Type::standard:
			lua_pushglobaltable(L);
			return true;
		case value::Type::table_idx: {
			lua_pushnil(L);
			lua_pushnil(L);
			for (int i = 0; i < v.index; ++i) {
				lua_pop(L, 1);
				if (!lua_next(L, -2)) {
					return false;
				}
			}
			lua_remove(L, -2);
			return true;
		}
		case value::Type::metatable:
			return !!lua_getmetatable(L, -1);
		case value::Type::uservalue:
			//TODO: 5.4֧�ֶ��uservalue
			lua_getuservalue(L, -1);
			return true;
		case value::Type::func_upvalue:
			return !!lua_getupvalue(L, -1, v.index);
		case value::Type::debugger_extand:
			if (!userdata_debugger_extand(L, -1)) {
				return false;
			}
			if (LUA_TSTRING != lua_rawgeti(L, -1, v.index)) {
				lua_pop(L, 2);
				return false;
			}
			if (LUA_TNIL == lua_gettable(L, -2)) {
				lua_pop(L, 2);
				return false;
			}
			lua_remove(L, -2);
			return true;
		}
		return false;
	}

	bool frame::push_value(lua_State* L, lua::Debug* ar, size_t value_idx)
	{
		int n = lua_gettop(L);
		if (!push_value(L, ar, values[value_idx])) {
			lua_settop(L, n);
			return false;
		}
		lua_copy(L, -1, n + 1);
		lua_settop(L, n + 1);
		return true;
	}

	void frame::extand_local(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		std::vector<var> vars;
		for (int n = 1;; n++)
		{
			const char* name = 0;
			switch (v.type) {
			case value::Type::upvalue: if (!get_upvalue(L, ar, n, &name)) goto finish; else break;
			case value::Type::local: if (!get_local(L, ar, n, &name)) goto finish; else break;
			case value::Type::vararg: if (!get_vararg(L, ar, n, &name)) goto finish; else break;
			default: return;
			}
			if (name) {
				vars.erase(
					std::remove_if(vars.begin(), vars.end(), [&](var const& v)->bool {
						return v.name == name;
					}), vars.end()
				);
				vars.emplace_back(var(L, n, name, -1, dbg->get_pathconvert()));
			}
			lua_pop(L, 1);
		}
finish:
		for (auto& var : vars) {
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, v.type, var.n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
		}
	}

	void frame::extand_global(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		int n = 0;
		std::set<var> vars;
		lua_pushglobaltable(L);
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (vars.size() >= 300) {
				lua_pop(L, 2);
				break;
			}
			var var(L, ++n, -2, -1, dbg->get_pathconvert());
			bool is_standard = standard.find(var.name) != standard.end();
			if ((is_standard && v.type == value::Type::standard)
				|| (!is_standard && v.type != value::Type::standard)) 
			{
				vars.insert(std::move(var));
			}
			lua_pop(L, 1);
		}

		for (auto& var : vars)
		{
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, value::Type::table_idx, var.n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
		}
		if (vars.size() == 300) {
			for (auto _ : res.Object())
			{
				res("name").String("...");
				res("value").String("");
				res("type").String("");
			}
		}
	}

	void frame::extand_table(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		int n = 0;
		std::set<var> vars;
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (vars.size() >= 300) {
				lua_pop(L, 2);
				break;
			}
			var var(L, ++n, -2, -1, dbg->get_pathconvert());
			vars.insert(std::move(var));
			lua_pop(L, 1);
		}

		for (auto& var : vars)
		{
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, value::Type::table_idx, var.n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
		}
		if (vars.size() == 300) {
			for (auto _ : res.Object())
			{
				res("name").String("...");
				res("value").String("");
				res("type").String("");
			}
		}
	}

	void frame::extand_metatable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		if (lua_getmetatable(L, -1)) {
			var var(L, 0, "[metatable]", -1, dbg->get_pathconvert());
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, value::Type::metatable, var.n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
			lua_pop(L, 1);
		}
	}

	void frame::extand_userdata(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		//TODO: 5.4֧�ֶ��uservalue
		if (lua_getuservalue(L, -1) != LUA_TNIL) {
			var var(L, 0, "[uservalue]", -1, dbg->get_pathconvert());
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, value::Type::uservalue, var.n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
		}
		lua_pop(L, 1);

		if (userdata_debugger_extand(L, -1)) {
			for (int n = 1;; ++n) {
				if (LUA_TSTRING != lua_rawgeti(L, -1, n)) {
					lua_pop(L, 1);
					break;
				}
				lua_pushvalue(L, -1);
				if (LUA_TNIL == lua_gettable(L, -3)) {
					lua_pop(L, 2);
					continue;
				}
				var var(L, n, -2, -1, dbg->get_pathconvert());
				for (auto _ : res.Object())
				{
					if (var.extand) {
						res("variablesReference").Int64(new_variable(v.self, value::Type::debugger_extand, n));
					}
					res("name").String(var.name);
					res("value").String(var.value);
					res("type").String(var.type);
				}
				lua_pop(L, 2);
			}
			lua_pop(L, 1);
		}
	}

	void frame::extand_function(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res)
	{
		for (int n = 1;; n++) {
			const char* name = lua_getupvalue(L, -1, n);
			if (!name)
				break;
			var var(L, n, name, -1, dbg->get_pathconvert());
			for (auto _ : res.Object())
			{
				if (var.extand) {
					res("variablesReference").Int64(new_variable(v.self, value::Type::func_upvalue, n));
				}
				res("name").String(var.name);
				res("value").String(var.value);
				res("type").String(var.type);
			}
			lua_pop(L, 1);
		}
	}

	void frame::get_variable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, int64_t valueId, wprotocol& res)
	{
		size_t value_idx = (valueId & 0xFFFF) - 1;
		if (value_idx >= values.size()) {
			return;
		}

		value v = values[value_idx];
		if (v.parent == -1) {
			switch (v.type) {
			case value::Type::local:
			case value::Type::vararg:
			case value::Type::upvalue:
				return extand_local(L, ar, dbg, v, res);
			case value::Type::global:
			case value::Type::standard:
				return extand_global(L, ar, dbg, v, res);
			case value::Type::watch:
				break;
			default:
				return;
			}
		}
		if (!push_value(L, ar, value_idx)) {
			return;
		}
		switch (lua_type(L, -1)) {
		case LUA_TTABLE:
			extand_metatable(L, ar, dbg, v, res);
			extand_table(L, ar, dbg, v, res);
			break;
		case LUA_TLIGHTUSERDATA:
			extand_metatable(L, ar, dbg, v, res);
			break;
		case LUA_TUSERDATA:
			extand_metatable(L, ar, dbg, v, res);
			extand_userdata(L, ar, dbg, v, res);
			break;
		case LUA_TFUNCTION:
			extand_function(L, ar, dbg, v, res);
			break;
		}
		lua_pop(L, 1);
	}

	bool frame::set_table(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue)
	{
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			std::string name = var::getName(L, -2);
			if (name == setvalue.name) {
				if (!push_setvalue(L, lua_type(L, -1), setvalue)) {
					lua_pop(L, 2);
					return false;
				}
				lua_remove(L, -2);
				lua_rawset(L, -3);
				return true;
			}
			lua_pop(L, 1);
		}
		return false;
	}

	bool frame::set_userdata(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue)
	{
		if (!userdata_debugger_extand(L, -1)) {
			return false;
		}
		lua_pushlstring(L, setvalue.name.data(), setvalue.name.size());
		if (LUA_TNIL == lua_gettable(L, -2)) {
			lua_pop(L, 2);
			return false;
		}
		if (!push_setvalue(L, lua_type(L, -1), setvalue)) {
			lua_pop(L, 2);
			return false;
		}
		lua_remove(L, -2);
		lua_rawset(L, -3);
		return true;
	}

	bool frame::set_local(lua_State* L, lua::Debug* ar, set_value& setvalue)
	{
		for (int n = 1;; n++) {
			const char* name = 0;
			if (!get_local(L, ar, n, &name)) {
				break;
			}
			if (name && setvalue.name == name) {
				if (!push_setvalue(L, lua_type(L, -1), setvalue)) {
					lua_pop(L, 1);
					return false;
				}
				lua_remove(L, -2);
				if (!lua_setlocal(L, (lua_Debug*)ar, n)) {
					lua_pop(L, 1);
					return false;
				}
				return true;
			}
		}
		return false;
	}

	bool frame::set_vararg(lua_State* L, lua::Debug* ar, set_value& setvalue)
	{
		for (int n = 1;; n++) {
			const char* name = 0;
			if (!get_vararg(L, ar, n, &name)) {
				break;
			}
			if (name && setvalue.name == name) {
				if (!push_setvalue(L, lua_type(L, -1), setvalue)) {
					lua_pop(L, 1);
					return false;
				}
				lua_remove(L, -2);
				if (!lua_setlocal(L, (lua_Debug*)ar, -n)) {
					lua_pop(L, 1);
					return false;
				}
				return true;
			}
		}
		return false;
	}

	bool frame::set_upvalue(lua_State* L, lua::Debug* ar, set_value& setvalue)
	{
		if (!lua_getinfo(L, "f", (lua_Debug*)ar)) {
			return false;
		}
		for (int n = 1;; n++) {
			const char* name = lua_getupvalue(L, -1, n);
			if (!name) {
				lua_pop(L, 1);
				return false;
			}
			if (name && setvalue.name == name) {
				if (!push_setvalue(L, lua_type(L, -1), setvalue)) {
					lua_pop(L, 2);
					return false;
				}
				lua_remove(L, -2);
				if (!lua_setupvalue(L, -2, n)) {
					lua_pop(L, 2);
					return false;
				}
				lua_pop(L, 1);
				return true;
			}
		}
		return false;
	}

	bool frame::set_global(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue)
	{
		lua_pushglobaltable(L);
		bool ok = set_table(L, ar, dbg, setvalue);
		lua_pop(L, 1);
		return ok;
	}

	bool frame::set_variable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue, int64_t valueId)
	{
		size_t value_idx = (valueId & 0xFFFF) - 1;
		if (value_idx >= values.size()) {
			return false;
		}

		value& v = values[value_idx];
		if (v.parent == -1) {
			switch (v.type) {
			case value::Type::local: return set_local(L, ar, setvalue);
			case value::Type::vararg: return set_vararg(L, ar, setvalue);
			case value::Type::upvalue: return set_upvalue(L, ar, setvalue);
			case value::Type::global:
			case value::Type::standard: return set_global(L, ar, dbg, setvalue);
			}
			return false;
		}

		if (!push_value(L, ar, value_idx)) {
			return false;
		}
		bool ok = false;
		switch (lua_type(L, -1)) {
		case LUA_TTABLE:
			ok = set_table(L, ar, dbg, setvalue);
			break;
		case LUA_TUSERDATA:
			ok = set_userdata(L, ar, dbg, setvalue);
			break;
		case LUA_TLIGHTUSERDATA:
		case LUA_TFUNCTION:
			break;
		}
		lua_pop(L, 1);
		return ok;
	}

	observer::observer(int threadId)
		: threadId(threadId)
		, frames()
	{ }

	void observer::reset(lua_State* L)
	{
		if (L) watch_table_clear(L);
		frames.clear();
	}

	frame* observer::create_or_get_frame(int frameId)
	{
		auto it = frames.find(frameId);
		if (it != frames.end()) {
			return &(it->second);
		}
		auto res = frames.insert(std::make_pair(frameId, frame(threadId, frameId)));
		return &(res.first->second);
	}

	int64_t observer::new_watch(lua_State* L, int idx, frame* frame, const std::string& expression)
	{
		watch_table(L);
		lua_pushlstring(L, expression.data(), expression.size());
		if (LUA_TNUMBER == lua_rawget(L, -2)) {
			int n = (int)lua_tointeger(L, -1);
			lua_pushvalue(L, idx);
			lua_rawset(L, -3);
			lua_pop(L, 1);
			return frame->new_variable(-1, value::Type::watch, n);
		}
		lua_pop(L, 1);
		int n = (int)(1 + luaL_len(L, -1));
		lua_pushvalue(L, idx);
		lua_rawseti(L, -2, n);

		lua_pushlstring(L, expression.data(), expression.size());
		lua_pushinteger(L, n);
		lua_rawset(L, -3);

		lua_pop(L, 1);
		return frame->new_variable(-1, value::Type::watch, n);
	}

	void observer::evaluate(lua_State* L, lua::Debug *ar, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		auto& args = req["arguments"];

		std::string context = "";
		if (args.HasMember("context")) {
			context = args["context"].Get<std::string>();
		}

		if (!args.HasMember("expression")) {
			dbg->response_error(req, "Error expression");
			return;
		}
		std::string expression = args["expression"].Get<std::string>();

		int nresult = 0;
		if (!vscode::evaluate(L, ar, ("return " + expression).c_str(), nresult, context == "repl"))
		{
			if (context != "repl")
			{
				dbg->response_error(req, lua_tostring(L, -1));
				lua_pop(L, 1);
				return;
			}
			if (!vscode::evaluate(L, ar, expression.c_str(), nresult, true))
			{
				dbg->response_error(req, lua_tostring(L, -1));
				lua_pop(L, 1);
				return;
			}
			dbg->response_success(req, [&](wprotocol& res)
			{
			});
			lua_pop(L, nresult);
			return;
		}

		dbg->response_success(req, [&](wprotocol& res)
		{
			std::vector<var> rets;
			for (int i = 0; i < nresult; ++i)
			{
				var var(L, i, "", i - nresult, dbg->get_pathconvert());
				rets.emplace_back(std::move(var));
			}
			int resIdx = lua_absindex(L, -nresult);
			if (var::canExtand(L, resIdx)) {
				int64_t reference = new_watch(L, resIdx, create_or_get_frame(frameId), expression);
				res("variablesReference").Int64(reference);
			}
			lua_pop(L, nresult);
			if (rets.size() == 0)
			{
				res("result").String("nil");
			}
			else if (rets.size() == 1)
			{
				res("result").String(rets[0].value);
			}
			else
			{
				std::string result = rets[0].value;
				for (int i = 1; i < (int)rets.size(); ++i)
				{
					result += ", " + rets[i].value;
				}
				res("result").String(result);
			}
		});
	}

	void observer::new_frame(lua_State* L, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		lua::Debug entry;
		if (!lua_getstack(L, frameId, (lua_Debug*)&entry)) {
			dbg->response_error(req, "Error retrieving stack frame");
			return;
		}
		frame* frame = create_or_get_frame(frameId);
		dbg->response_success(req, [&](wprotocol& res)
		{
			frame->new_scope(L, &entry, res);
		});
	}

	void observer::get_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		auto it = frames.find(frameId);
		if (it == frames.end()) {
			dbg->response_error(req, "Error retrieving stack frame");
			return;
		}
		lua::Debug entry;
		if (!lua_getstack(L, frameId, (lua_Debug*)&entry)) {
			dbg->response_error(req, "Error retrieving variables");
			return;
		}
		dbg->response_success(req, [&](wprotocol& res)
		{
			res("variables").StartArray();
			it->second.get_variable(L, &entry, dbg, valueId, res);
			res.EndArray();
		});
	}

	void observer::set_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		auto& args = req["arguments"];
		auto it = frames.find(frameId);
		if (it == frames.end()) {
			dbg->response_error(req, "Error retrieving stack frame");
			return;
		}
		lua::Debug entry;
		if (!lua_getstack(L, frameId, (lua_Debug*)&entry)) {
			dbg->response_error(req, "Error retrieving variables");
			return;
		}

		set_value setvalue;
		setvalue.name = args["name"].Get<std::string>();
		setvalue.value = args["value"].Get<std::string>();
		if (!it->second.set_variable(L, &entry, dbg, setvalue, valueId)) {
			dbg->response_error(req, "Failed set variable");
			return;
		}
		dbg->response_success(req, [&](wprotocol& res)
		{
			res("value").String(setvalue.value);
			res("type").String(setvalue.type);
		});
	}
}
