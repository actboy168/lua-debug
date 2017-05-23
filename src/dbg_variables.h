#pragma once

#include <stdint.h>
#include <string> 
#include "dbg_pathconvert.h"

struct lua_State;
struct lua_Debug;

namespace vscode
{
	class  wprotocol;
	enum class var_type {
		none = 0,
		local = 1,
		vararg = 2,
		upvalue = 3,
		global = 4,
		standard = 5,
		watch = 6,
	};

	struct variable {
		std::string name;
		std::string value;
		std::string type;
		int64_t     reference;

		variable()
			: name()
			, value()
			, reference(0)
		{ }

		bool operator < (const variable& that) const
		{
			return name < that.name;
		}

		void set(lua_State *L, int idx, pathconvert& pathconvert); 

	private:
		void set_type(lua_State *L, int idx);
		void set_value(lua_State *L, int idx, pathconvert& pathconvert);
	};

	class variables
	{
	public:
		variables(wprotocol& res, lua_State* L, lua_Debug* ar, int fix = 0);
		~variables();
		bool push(const variable& var);
		void push_value(var_type type, int depth, int64_t pos, pathconvert& pathconvert);
		void each_table(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert);
		void each_function(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert);
		void each_userdata(int idx, int level, int64_t pos, var_type type, pathconvert& pathconvert);

	public:
		static bool set_value(lua_State* L, lua_Debug* ar, var_type type, int depth, int64_t pos, variable& var);
		static bool find_value(lua_State* L, lua_Debug* ar, var_type type, int depth, int64_t pos);

	private:
		wprotocol& res;
		lua_State* L;
		lua_Debug* ar;
		size_t n;
		int checkstack;
	};

	bool can_extand(lua_State *L, int idx);
}
