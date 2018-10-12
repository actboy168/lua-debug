#pragma once

#include <debugger/lua.h>
#include <debugger/breakpoint.h>
#include <debugger/observer.h>
#include <debugger/thunk/thunk.h>

namespace vscode
{
	class debugger_impl;
	class breakpointMgr;

	struct luathread {
		enum class step {
			in = 1,
			over = 2,
			out = 3,
		};

		int            id;
		bool           enable;
		bool           release;
		debugger_impl& dbg;
		lua_State*     L;
		std::unique_ptr<thunk> thunk_hook;
		std::unique_ptr<thunk> thunk_panic;
		lua_CFunction  oldpanic;

		step           step_;
		int            stepping_target_level_;
		int            stepping_current_level_;
		lua_State*     stepping_lua_state_;
		bool           has_function;
		bool           has_breakpoint;
		bp_source*     cur_function;
		observer       ob_;

		luathread(int id, debugger_impl& dbg, lua_State* L);
		~luathread();

		void install_hook(int mask);

		void release_thread();
		bool enable_thread();
		void disable_thread();
		void set_step(step step);
		bool is_step(step step);
		bool check_step(lua_State* L);
		void step_in();
		void step_over(lua_State* L);
		void step_out(lua_State* L);
		void hook_callret(debug& debug);
		void hook_line(debug& debug, breakpointMgr& breakpointmgr);
		void update_breakpoint();

		void reset_session(lua_State* L);
		void evaluate(lua_State* L, lua::Debug *ar, debugger_impl& dbg, rprotocol& req, int frameId);
		void new_frame(debug& debug, debugger_impl& dbg, rprotocol& req, int frameId);
		void get_variable(debug& debug, debugger_impl& dbg, rprotocol& req, int64_t valueId, int frameId);
		void set_variable(debug& debug, debugger_impl& dbg, rprotocol& req, int64_t valueId, int frameId);
	};
}
