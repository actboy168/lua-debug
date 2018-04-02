#pragma once

#include <stdint.h>  
#include <functional>
#include <map>
#include <set>
#include <vector> 
#include <mutex>
#include <atomic>
#include <rapidjson/document.h>
#include "dbg_lua.h"	 
#include "dbg_breakpoint.h"	 
#include "dbg_evaluate.h"
#include "dbg_pathconvert.h"
#include "dbg_protocol.h"
#include "debugger.h"
#include "dbg_redirect.h"
#include "dbg_observer.h"
#include "dbg_config.h"

namespace vscode
{
	struct io;
	class rprotocol;
	class wprotocol;
	struct dbg_thread;

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
		debugger_impl(io* io, threadmode mode);
		~debugger_impl();
		void close();
		void io_close();
		void hook(lua_State* L, lua::Debug* ar);
		void exception(lua_State* L);
		void run_stopped(lua_State* L, lua::Debug* ar, const char* reason);
		void run_idle();
		void update();
		void wait_attach();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len, lua_State* L);
		bool set_config(int level, const std::string& cfg, std::string& err);

		void set_state(state state);
		bool is_state(state state) const;
		void set_step(step step);
		bool is_step(step step);
		void step_in();
		void step_over(lua_State* L, lua::Debug* ar);
		void step_out(lua_State* L, lua::Debug* ar);
		bool check_step(lua_State* L, lua::Debug* ar);
		bool check_breakpoint(lua_State *L, lua::Debug *ar);
		void redirect_stdout();
		void redirect_stderr();
		pathconvert& get_pathconvert();
		rprotocol io_input();
		void io_output(const wprotocol& wp);

	private:
		bool request_initialize(rprotocol& req);
		bool request_set_breakpoints(rprotocol& req);
		bool request_attach(rprotocol& req);
		bool request_attach_done(rprotocol& req);
		bool request_configuration_done(rprotocol& req);
		bool request_disconnect(rprotocol& req);
		bool request_pause(rprotocol& req);
		bool request_set_exception_breakpoints(rprotocol& req);

	private:
		bool request_thread(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_stack_trace(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_source(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_scopes(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_variables(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_set_variable(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_stepin(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_stepout(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_next(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_continue(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_evaluate(rprotocol& req, lua_State *L, lua::Debug *ar);
		bool request_exception_info(rprotocol& req, lua_State *L, lua::Debug *ar);

	private:
		void event_stopped(const char *msg);
		void event_thread(bool started);
		void event_terminated();
		void event_initialized();
		void event_capabilities();

	public:
		void response_error(rprotocol& req, const char *msg);
		void response_success(rprotocol& req);
		void response_success(rprotocol& req, std::function<void(wprotocol&)> body);

	private:
		void response_initialize(rprotocol& req);
		void response_thread(rprotocol& req);
		void response_source(rprotocol& req, const char* content);

	private:
		void detach_all();
		bool update_main(rprotocol& req, bool& quit);
		bool update_hook(rprotocol& req, lua_State *L, lua::Debug *ar, bool& quit);
		void initialize_sourcemaps();
		void update_redirect();

	private:
		int64_t            seq;
		io*                network_;
		state              state_;
		step               step_;
		int                stepping_target_level_;
		int                stepping_current_level_;
		lua_State*         stepping_lua_state_;
		breakpoint         breakpoints_;
		std::vector<stack> stack_;
		pathconvert        pathconvert_;
		custom*            custom_;
		lua_Hook           thunk_hook_;
		bool               has_source_;
		bp_source*         cur_source_;
		bool               exception_;
		std::set<lua_State*> hookL_;
		dbg_thread*        thread_;
		std::atomic<bool>  allowhook_;
		std::function<void()> on_attach_;
		std::string        console_;
		std::unique_ptr<redirector> stdout_;
		std::unique_ptr<redirector> stderr_;
		rprotocol          initproto_;
		observer           ob_;
		config             config_;
		bool               nodebug_;
		std::map<std::string, std::function<bool(rprotocol&)>>                            main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua::Debug *ar)>> hook_dispatch_;
	};
}
