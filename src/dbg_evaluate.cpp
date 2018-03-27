#include "dbg_evaluate.h"
#include "dbg_lua.h"	 
#include <base/util/format.h>
#include <vector>
#include <map>

namespace vscode
{
	struct strbuilder
		: public std::vector<std::string>
	{
		strbuilder()
			: n(0)
		{ }

		void clear()
		{
			n = 0;
			std::vector<std::string>::clear();
		}

		strbuilder& operator +=	(const std::string& s)
		{
			push_back(s);
			n += s.size();
			return *this;
		}

		strbuilder& operator +=	(std::string&& s)
		{
			push_back(std::forward<std::string>(s));
			n += s.size();
			return *this;
		}

		strbuilder& operator +=	(strbuilder&& sb)
		{
			for (const std::string& s : sb)
			{
				(*this) += std::move(s);
			}
			sb.clear();
			return *this;
		}

		operator std::string()
		{
			std::string r;
			r.reserve(n);
			for (const std::string& s : *this)
			{
				r += std::move(s);
			}
			return std::move(r);
		}

		size_t n;
	};

	
	bool evaluate(lua_State* L, lua::Debug *ar, const char* script, int& nresult, bool writeable)
	{
		strbuilder code;
		strbuilder local;
		int startstack = lua_gettop(L);
		int srcfunc = 0;
		std::map<std::string, int> id_local;
		std::map<std::string, int> id_upvalue;

		lua_checkstack(L, 16);
		for (int i = 1;; ++i)
		{
			const char* name = lua_getlocal(L, (lua_Debug*)ar, i);
			if (!name) break;
			if (name[0] == '(') { lua_pop(L, 1); continue; }
			code += base::format("local %s\n", name);
			if (writeable)
				local += base::format("[%d]=%s,", i, name);
			id_local[name] = lua_gettop(L);
			if (i % 10 == 0)
			{
				lua_checkstack(L, 10);
			}
		}

		if (lua_getinfo(L, "f", (lua_Debug*)ar))
		{
			srcfunc = lua_gettop(L);
			for (int i = 1;; ++i)
			{
				const char* name = lua_getupvalue(L, -1, i);
				if (!name) break;
				code += base::format("local %s\n", name);
				id_upvalue[name] = i;
				lua_pop(L, 1);
			}
		}
		code += "return ";
		if (writeable)
		{
			code += "function() return{";
			code += local;
			code += "}\nend\n,";
		}
		code += "function(...)\n";
		code += script;
		code += "\nend";
		std::string codestr = code;
		if (luaL_loadbuffer(L, codestr.data(), codestr.size(), "=(debug)"))
		{
			return false;
		}
		// todo: pcall?
		lua_call(L, 0, writeable? 2: 1);
		for (int i = 1;; ++i)
		{
			const char* name = lua_getupvalue(L, -1, i);
			if (!name) break;
			lua_pop(L, 1);
			{
				auto it = id_local.find(name);
				if (it != id_local.end())
				{
					lua_pushvalue(L, it->second);
					if (!lua_setupvalue(L, -2, i))
						lua_pop(L, 1);
					continue;
				}
			}
			if (writeable)
			{
				auto it = id_upvalue.find(name);
				if (it != id_upvalue.end())
				{
					lua_upvaluejoin(L, -1, i, srcfunc, it->second);
					continue;
				}
			}
			else
			{
				auto it = id_upvalue.find(name);
				if (it != id_upvalue.end())
				{
					if (lua_getupvalue(L, srcfunc, it->second))
					{
						if (!lua_setupvalue(L, -2, i))
							lua_pop(L, 1);
					}
					continue;
				}
			}
		}

		int vararg = 1;
		for (;; ++vararg)
		{
			if (!lua_getlocal(L, (lua_Debug*)ar, -vararg))
				break;
		}
		int start = lua_gettop(L) - vararg;
		if (lua_pcall(L, vararg - 1, LUA_MULTRET, 0))
		{
			lua_rotate(L, startstack + 1, 1);
			lua_settop(L, startstack + 1);
			return false;
		}
		nresult = lua_gettop(L) - start;
		if (writeable)
		{
			lua_rotate(L, start, 1);
			lua_call(L, 0, 1);

			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				if (!lua_setlocal(L, (lua_Debug*)ar, (int)lua_tointeger(L, -2))) {
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);
		}
		lua_rotate(L, startstack + 1, nresult);
		lua_settop(L, startstack + nresult);
		return true;
	}
}
