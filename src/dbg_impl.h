#pragma once

#include <stdint.h>
#include <lua.hpp>	  
#include <functional>
#include <map>
#include <vector> 
#include <asmjit/asmjit.h>	
#include <rapidjson/document.h>
#include "dbg_breakpoint.h"	 
#include "dbg_evaluate.h"
#include "dbg_path.h" 
#include "dbg_custom.h"	
#include "dbg_pathconvert.h" 
#include "dbg_enum.h"   
#include "dbg_watchs.h"	  
#include "dbg_protocol.h"

namespace vscode
{
	class io;
	class rprotocol;
	class wprotocol;

	class debugger_impl
	{
		friend class pathconvert;
	public:	
		enum class step {
			in = 1,
			over = 2,
			out = 3,
		};

		struct stack {
			int depth;
			int64_t reference;
		};

	public:
		debugger_impl(lua_State* L, io* io);
		~debugger_impl();
		void hook(lua_State *L, lua_Debug *ar);
		void exception(lua_State *L, const char* msg);
		void loop(lua_State *L, lua_Debug *ar);
		void update();
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len);

		void set_state(state state);
		bool is_state(state state);
		void set_step(step step);
		bool is_step(step step);
		void step_in();
		void step_over(lua_State* L, lua_Debug* ar);
		void step_out(lua_State* L, lua_Debug* ar);
		bool check_step(lua_State* L, lua_Debug* ar);
		bool check_breakpoint(lua_State *L, lua_Debug *ar);

	private:
		bool request_initialize(rprotocol& req);
		bool request_set_breakpoints(rprotocol& req);
		bool request_configuration_done(rprotocol& req);
		bool request_launch(rprotocol& req);
		bool request_attach(rprotocol& req);
		bool request_disconnect(rprotocol& req);
		bool request_pause(rprotocol& req);
		bool request_set_exception_breakpoints(rprotocol& req);

	private:
		bool request_thread(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_stack_trace(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_source(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_scopes(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_variables(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_set_variable(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_stepin(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_stepout(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_next(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_continue(rprotocol& req, lua_State *L, lua_Debug *ar);
		bool request_evaluate(rprotocol& req, lua_State *L, lua_Debug *ar);

	private:
		void event_stopped(const char *msg, const char* text = nullptr);
		void event_thread(bool started);
		void event_terminated();
		void event_initialized();

	public:
		template <class T>
		void event_output(const std::string& category, const T& msg)
		{
			wprotocol res;
			for (auto _ : res.Object())
			{
				res("type").String("event");
				res("seq").Int64(seq++);
				res("event").String("output");
				for (auto _ : res("body").Object())
				{
					res("category").String(category);
					res("output").String(msg);
				}
			}
			network_->output(res);
		}

	private:
		void response_error(rprotocol& req, const char *msg);
		void response_success(rprotocol& req);
		void response_success(rprotocol& req, std::function<void(wprotocol&)> body);
		void response_initialize(rprotocol& req);
		void response_thread(rprotocol& req);
		void response_source(rprotocol& req, const char* content);

	private:
		void create_asmjit();
		void release_asmjit();
		void open();
		void close();
		bool update_main(rprotocol& req, bool& quit);
		bool update_hook(rprotocol& req, lua_State *L, lua_Debug *ar, bool& quit);
		void update_launch();
		void initialize_sourcemaps(rapidjson::Value& args);
		bool request_launch_done(rprotocol& req);

	private:
		lua_State*         GL;
		int64_t            seq;
		io*                network_;
		state              state_;
		step               step_;
		int                stepping_stacklevel_;
		lua_State*         stepping_lua_state_;
		int                stacklevel_;
		breakpoint         breakpoints_;
		std::vector<stack> stack_;
		watchs             watch_;
		pathconvert        pathconvert_;
		custom*            custom_;
		asmjit::JitRuntime asm_jit_;
		lua_Hook           asm_func_;
		rprotocol          cache_launch_;
		bool               has_source_;
		bp_source*         cur_source_;
		bool               exception_;
		bool               launch_;
		std::map<std::string, std::function<bool(rprotocol&)>>                            main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua_Debug *ar)>> hook_dispatch_;
	};
}
