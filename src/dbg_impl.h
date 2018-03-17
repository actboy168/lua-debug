#pragma once

#include <stdint.h>  
#include <functional>
#include <map>
#include <vector> 
#include <mutex>
#include <atomic>
#include <rapidjson/document.h>
#include "lua_compatibility.h"	 
#include "dbg_breakpoint.h"	 
#include "dbg_evaluate.h"
#include "dbg_path.h" 
#include "dbg_custom.h"	
#include "dbg_pathconvert.h" 
#include "dbg_enum.h"   
#include "dbg_watchs.h"
#include "dbg_protocol.h"
#include "debugger.h"
#include "dbg_redirect.h"

namespace vscode
{
	class io;
	class rprotocol;
	class wprotocol;

	struct dbg_thread
	{
		virtual threadmode mode() const = 0;
		virtual void start() = 0;
		virtual void update() = 0;
		virtual void lock() = 0;
		virtual bool try_lock() = 0;
		virtual void unlock() = 0;
	};

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
		debugger_impl(io* io, threadmode mode, coding coding);
		~debugger_impl();
		void hook(lua_State *L, lua::Debug *ar);
		void exception(lua_State *L, const char* msg);
		void run_stopped(lua_State *L, lua::Debug *ar);
		void run_idle();
		void update();
		void wait_attach();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void set_coding(coding coding);
		void output(const char* category, const char* buf, size_t len, lua_State* L);

		void set_state(state state);
		bool is_state(state state);
		void set_step(step step);
		bool is_step(step step);
		void step_in();
		void step_over(lua_State* L, lua::Debug* ar);
		void step_out(lua_State* L, lua::Debug* ar);
		bool check_step(lua_State* L, lua::Debug* ar);
		bool check_breakpoint(lua_State *L, lua::Debug *ar);

	private:
		bool request_initialize(rprotocol& req);
		bool request_set_breakpoints(rprotocol& req);
		bool request_attach(rprotocol& req);
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

	private:
		void event_stopped(const char *msg, const char* text = nullptr);
		void event_thread(bool started);
		void event_terminated();
		void event_initialized();
		void event_capabilities();

	public:
		template <class T>
		void event_output(const std::string& category, const T& msg, lua_State* L = nullptr)
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
					if (console_ == "ansi") {
						res("output").String(vscode::a2u(msg));
					}
					else {
						res("output").String(msg);
					}

					lua::Debug entry;
					if (L && lua_getstack(L, 1, (lua_Debug*)&entry))
					{
						int status = lua_getinfo(L, "Sln", (lua_Debug*)&entry);
						assert(status);
						const char *src = entry.source;
						if (memcmp(src, "=[C]", 4) == 0) 
						{
						}
						else if (*src == '@' || *src == '=')
						{
							std::string path;
							if (pathconvert_.get(src, path))
							{
								for (auto _ : res("source").Object())
								{
									res("name").String(w2u(fs::path(u2w(path)).filename().wstring()));
									res("path").String(path);
									res("sourceReference").Int64(0);
								};
								res("line").Int(entry.currentline);
								res("column").Int(0);
							}
						}
						else
						{
							intptr_t reference = (intptr_t)src;
							for (auto _ : res("source").Object())
							{
								res("name").String("<Memory funtion>");
								res("sourceReference").Int64(reference);
							}
							res("name").String(entry.name ? entry.name : "?");
							res("line").Int(entry.currentline);
							res("column").Int(0);
						}
					}
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
		void open_hook(lua_State* L);
		void close_hook();
		bool update_main(rprotocol& req, bool& quit);
		bool update_hook(rprotocol& req, lua_State *L, lua::Debug *ar, bool& quit);
		void initialize_sourcemaps(rapidjson::Value& args);
		void init_redirector(rprotocol& req, lua_State* L);
		void update_redirect();

#if !defined(DEBUGGER_DISABLE_LAUNCH)
		bool request_launch(rprotocol& req);
		bool request_launch_done(rprotocol& req);
		void update_launch();
#endif

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
		std::unique_ptr<watchs> watch_;
		pathconvert        pathconvert_;
		custom*            custom_;
		lua_Hook           thunk_;
		bool               has_source_;
		bp_source*         cur_source_;
		bool               exception_;
		lua_State*         attachL_;
		lua_State*         hookL_;
		dbg_thread*        thread_;
		std::atomic<bool>  allowhook_;
		std::function<void()> attach_callback_;
		std::string        console_;
		std::unique_ptr<redirector> stdout_;
		std::unique_ptr<redirector> stderr_;
#if !defined(DEBUGGER_DISABLE_LAUNCH)
		rprotocol          cache_launch_;
		lua_State*         launchL_;
#endif
		std::map<std::string, std::function<bool(rprotocol&)>>                            main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua::Debug *ar)>> hook_dispatch_;
	};
}
