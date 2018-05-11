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
#include <debugger/pathconvert.h>
#include <debugger/protocol.h>
#include <debugger/debugger.h>
#include <debugger/redirect.h>
#include <debugger/config.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/osthread.h>
#include <base/util/string_view.h>

namespace vscode
{
	class rprotocol;
	class wprotocol;
	struct luathread;
	typedef std::map<std::string_view, std::string_view> translator_t;

	class debugger_impl
	{
		friend class pathconvert;

	public:
		debugger_impl(io::base* io);
		~debugger_impl();
		bool open_schema(const std::wstring& path);
		void close();
		void io_close();
		void panic(luathread* thread, lua_State* L);
		void hook(luathread* thread, lua_State* L, lua::Debug* ar);
		void exception(lua_State* L, eException exceptionType, int level);
		void exception_nolock(luathread* thread, lua_State* L, eException exceptionType, int level);
		void run_stopped(luathread* thread, lua_State* L, lua::Debug* ar, const char* reason);
		void run_idle();
		void update();
		void wait_client();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L, bool remove);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len, lua_State* L = nullptr, lua::Debug* ar = nullptr);
		bool set_config(int level, const std::string& cfg, std::string& err);
		void terminate_on_disconnect(); 
		void on_disconnect();

		void set_state(eState state);
		bool is_state(eState state) const;
		void open_redirect(eRedirect type, lua_State* L);
		pathconvert& get_pathconvert();
		rprotocol io_input();
		void io_output(const wprotocol& wp);
		luathread* find_luathread(lua_State* L);
		luathread* find_luathread(int threadid);

		void setlang(const std::string_view& locale);
		std::string_view LANG(const std::string_view& text);

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
		void event_stopped(luathread* thread, const char *msg);
		void event_terminated();
		void event_initialized();
		void event_capabilities();

	public:
		void event_thread(luathread* thread, bool started);
		void event_breakpoint(const char* reason, bp_source* src, bp_breakpoint* bp);

	public:
		void response_error(rprotocol& req, const char *msg);
		void response_success(rprotocol& req);
		void response_success(rprotocol& req, std::function<void(wprotocol&)> body);

	private:
		void response_initialize(rprotocol& req);
		void response_thread(rprotocol& req);
		void response_source(rprotocol& req, const char* content);

	private:
		void detach_all(bool release);
		bool update_main(rprotocol& req, bool& quit);
		bool update_hook(rprotocol& req, lua_State *L, lua::Debug *ar, bool& quit);
		void initialize_pathconvert();
		void update_redirect();

	private:
		custom*                     custom_;
		eCoding                     consoleSourceCoding_;
		eCoding                     consoleTargetCoding_;
		std::function<void()>       on_clientattach_;
		std::unique_ptr<redirector> stdout_;
		std::unique_ptr<redirector> stderr_;
		rprotocol                   initproto_;
		config                      config_;
		bool                        nodebug_;
		translator_t*               translator_;
		std::map<std::string, std::function<bool(rprotocol&)>>                             main_dispatch_;
		std::map<std::string, std::function<bool(rprotocol&, lua_State*, lua::Debug *ar)>> hook_dispatch_;

		osthread             thread_;
		io::base*            network_;
		schema               schema_;
		breakpoint           breakpoints_;
		pathconvert          pathconvert_;
		std::map<int, std::unique_ptr<luathread>> luathreads_;

		int64_t              seq;
		int                  next_threadid_;
		std::set<eException> exception_;
		eState               state_;
	};
}
