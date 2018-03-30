#pragma once

#include <vector>
#include <map>
#include "dbg_protocol.h"
#include "dbg_pathconvert.h"
#include "dbg_lua.h"

namespace vscode {
	class debugger_impl;

	struct value {
		enum class Type {
			local,
			vararg,
			upvalue,
			global,
			standard,
			watch,

			table_int, // todo
			table_str, // todo
			table_idx,
			metatable,
			uservalue,
			func_upvalue,
			debugger_extand,
		};
		size_t     self;
		size_t     parent;
		Type       type;
		int        index;
	};

	struct set_value {
		std::string name;
		std::string value;
		std::string type;
	};

	struct frame {
		std::vector<value> values;
		int frameId;
		int64_t new_variable(size_t parent, value::Type type, int index);

		frame(int frameId);
		void new_scope(lua_State* L, lua::Debug* ar, wprotocol& res);
		void clear();
		bool push_value(lua_State* L, lua::Debug* ar, size_t value_idx);
		bool push_value(lua_State* L, lua::Debug* ar, const value& v);

		void extand_local(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void extand_global(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void extand_table(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void extand_metatable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void extand_userdata(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void extand_function(lua_State* L, lua::Debug* ar, debugger_impl* dbg, value const& v, wprotocol& res);
		void get_variable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, int64_t valueId, wprotocol& res);

		bool set_table(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue);
		bool set_userdata(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue);
		bool set_local(lua_State* L, lua::Debug* ar, set_value& setvalue);
		bool set_vararg(lua_State* L, lua::Debug* ar, set_value& setvalue);
		bool set_upvalue(lua_State* L, lua::Debug* ar, set_value& setvalue);
		bool set_global(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue);
		bool set_variable(lua_State* L, lua::Debug* ar, debugger_impl* dbg, set_value& setvalue, int64_t valueId);
	};

	struct observer {
		std::map<int, frame> frames;
		frame                watch_frame;

		observer();
		void    reset(lua_State* L = nullptr);
		frame*  create_or_get_frame(int frameId);
		void    new_frame(lua_State* L, debugger_impl* dbg, rprotocol& req);
		int64_t new_watch(lua_State* L, frame* frame, const std::string& expression);
		void    evaluate(lua_State* L, lua::Debug *ar, debugger_impl* dbg, rprotocol& req);
		void    get_variable(lua_State* L, debugger_impl* dbg, rprotocol& req);
		void    set_variable(lua_State* L, debugger_impl* dbg, rprotocol& req);
	};
}
