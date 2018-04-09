#pragma once

#include <stdint.h>  
#include <functional>
#include <map>
#include <set>
#include <vector> 
#include <mutex>
#include <atomic>
#include <stdint.h>
#include <rapidjson/document.h>
#include <debugger/lua.h>
#include <debugger/breakpoint.h>	 
#include <debugger/evaluate.h>
#include <debugger/pathconvert.h>
#include <debugger/protocol.h>
#include <debugger/debugger.h>
#include <debugger/redirect.h>
#include <debugger/observer.h>
#include <debugger/config.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>

namespace vscode
{
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

		struct lua_thread {
			int            id;
			debugger_impl* dbg;
			lua_State*     L;
			lua_Hook       thunk_hook;
			lua_CFunction  thunk_panic;
			lua_CFunction  oldpanic;

			step           step_;
			int            stepping_target_level_;
			int            stepping_current_level_;
			lua_State*     stepping_lua_state_;
			bool           has_source_;
			bp_source*     cur_source_;

			lua_thread(int id, debugger_impl* dbg, lua_State* L);
			~lua_thread();

			void set_step(step step);
			bool is_step(step step);
			bool check_step(lua_State* L, lua::Debug* ar);
			void step_in();
			void step_over(lua_State* L, lua::Debug* ar);
			void step_out(lua_State* L, lua::Debug* ar);
			void hook_call(lua_State* L, lua::Debug* ar);
			void hook_return(lua_State* L, lua::Debug* ar);
		};

	public:
		debugger_impl(io::base* io);
		~debugger_impl();
		bool open_schema(const std::wstring& path);
		void close();
		void io_close();
		void hook(lua_thread* thread, lua_State* L, lua::Debug* ar);
		void exception(lua_State* L);
		void exception(lua_thread* thread, lua_State* L);
		void run_stopped(lua_thread* thread, lua_State* L, lua::Debug* ar, const char* reason);
		void run_idle();
		void update();
		void wait_attach();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len, lua_State* L = nullptr, lua::Debug* ar = nullptr);
		bool set_config(int level, const std::string& cfg, std::string& err);

		void set_state(state state);
		bool is_state(state state) const;
		bool check_breakpoint(lua_thread* thread, lua_State *L, lua::Debug *ar);
		void redirect_stdout();
		void redirect_stderr();
		pathconvert& get_pathconvert();
		rprotocol io_input();
		void io_output(const wprotocol& wp);
		lua_thread* find_luathread(lua_State* L);
		lua_thread* find_luathread(int threadid);

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
		void event_stopped(lua_thread* thread, const char *msg);
		void event_thread(lua_thread* thread, bool started);
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
		void initialize_pathconvert();
		void update_redirect();

	private:
		int64_t            seq;
		io::base*          network_;
		schema             schema_;
		state              state_;
		breakpoint         breakpoints_;
		std::vector<stack> stack_;
		pathconvert        pathconvert_;
		custom*            custom_;
		dbg_thread*        thread_;
		std::function<void()> on_attach_;
		std::string        console_;
		std::unique_ptr<redirector> stdout_;
		std::unique_ptr<redirector> stderr_;
		rprotocol          initproto_;
		config             config_;
		bool               nodebug_;
		int                threadid_;
		bool               exception_;
		observer           ob_;
		std::map<int, std::unique_ptr<lua_thread>>                                         luathreads_;
		std::map<std::string, std::function<bool(rprotocol&)>>                             main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua::Debug *ar)>> hook_dispatch_;

	};
}
