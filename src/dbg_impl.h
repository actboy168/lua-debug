#pragma once

#include <stdint.h>
#include <lua.hpp>	  
#include <filesystem>
#include <functional>
#include <map>
#include <vector>
#include "dbg_breakpoint.h"	 
#include "dbg_redirect.h"  
#include "dbg_evaluate.h"

namespace fs = std::tr2::sys;

namespace vscode
{
	class network;
	class rprotocol;
	class wprotocol;

	class debugger_impl
	{
	public:
		enum state {
			birth = 1,
			initialized = 2,
			stepping = 3,
			running = 4,
			terminated = 5,
		};

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
		debugger_impl(lua_State* L, const char* ip, uint16_t port);
		void hook(lua_State *L, lua_Debug *ar);
		void update();
		void set_schema(const char* file);

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
		void event_stopped(const char *msg);
		void event_thread(bool started);
		void event_terminated();
		void event_initialized();

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

		void response_error(rprotocol& req, const char *msg);
		void response_success(rprotocol& req);
		void response_success(rprotocol& req, std::function<void(wprotocol&)> body);
		void response_initialized(rprotocol& req);
		void response_thread(rprotocol& req);
		void response_source(rprotocol& req, const char* content);

	private:
		void open();
		void close();
		static void debughook(lua_State *L, lua_Debug *ar);
		void update_redirect();
		bool update_main(rprotocol& req, bool& quit);
		bool update_hook(rprotocol& req, lua_State *L, lua_Debug *ar, bool& quit);

	private:
		lua_State*         GL;
		int64_t            seq;
		network*           network_;
		state              state_;
		step               step_;
		int                stepping_stacklevel_;
		lua_State*         stepping_lua_state_;
		int                stacklevel_;
		breakpoint         breakpoints_;
		fs::path           workingdir_;
		std::vector<stack> stack_;
		watchs             watch_;
		redirector         redirect_;
		std::map<std::string, std::function<bool(rprotocol&)>>                            main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua_Debug *ar)>> hook_dispatch_;
	};
}
