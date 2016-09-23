#include "dbg_impl.h"
#include "dbg_protocol.h"  
#include "dbg_network.h"	 
#include "dbg_format.h"
#include <thread>

namespace vscode
{
	static debugger_impl* global_debugger;
	void debugger_impl::debughook(lua_State *L, lua_Debug *ar)
	{
		if (!ar || !global_debugger)
			return;
		global_debugger->hook(L, ar);
	}

	void debugger_impl::open()
	{
		redirect_.open();
		global_debugger = this;
		stacklevel_ = 0;
		lua_sethook(GL, debugger_impl::debughook, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
	}

	void debugger_impl::close()
	{
		redirect_.close();
		global_debugger = 0;
		lua_sethook(GL, 0, 0, 0);
		breakpoints_.clear();
		stack_.clear();
		workingdir_.clear();
		seq = 1;
		stacklevel_ = 0;
	}

	void debugger_impl::update_redirect()
	{
		size_t n = 0;
		n = redirect_.peek_stdout();
		if (n > 1)
		{
			hybridarray<char, 1024> buf(n);
			redirect_.read_stdout(buf.data(), buf.size());
			event_output("stdout", buf);
		}
		n = redirect_.peek_stdout();
		if (n > 0)
		{
			hybridarray<char, 1024> buf(n);
			redirect_.read_stdout(buf.data(), buf.size());
			event_output("stderr", buf);
		}
	}

	bool debugger_impl::update_main(rprotocol& req, bool& quit)
	{
		auto it = main_dispatch_.find(req["command"].Get<std::string>());
		if (it != main_dispatch_.end())
		{
			quit = it->second(req);
			return true;
		}
		return false;
	}

	bool debugger_impl::update_hook(rprotocol& req, lua_State *L, lua_Debug *ar, bool& quit)
	{
		auto it = hook_dispatch_.find(req["command"].Get<std::string>());
		if (it != hook_dispatch_.end())
		{
			quit = it->second(req, L, ar);
			return true;
		}
		return false;
	}

	void debugger_impl::hook(lua_State *L, lua_Debug *ar)
	{
		if (ar->event == LUA_HOOKCALL)
		{
			stacklevel_++;
			return;
		}
		if (ar->event == LUA_HOOKRET)
		{
			stacklevel_--;
			return;
		}
		if (ar->event != LUA_HOOKLINE)
			return;
		if (is_state(state::terminated)) {
			return;
		}

		bool bp = false;
		if (is_state(state::running)) {
			if (!check_breakpoint(L, ar)) {
				return;
			}
			bp = true;
		}

		if (is_state(state::stepping)) {
			if (is_step(step::out) || is_step(step::over)) {
				if (!check_breakpoint(L, ar)) {
					if (!check_step(L, ar)) {
						return;
					}
				}
				else {
					bp = true;
				}
			}
			if (bp)
				event_stopped("breakpoint");
			else
				event_stopped("step");
			step_in();
		}

		bool quit = false;
		while (!quit)
		{
			network_->update(0);
			update_redirect();

			rprotocol req = network_->input();
			if (req.IsNull()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			if (req["type"] != "request") {
				continue;
			}
			if (update_main(req, quit)) {
				continue;
			}
			if (update_hook(req, L, ar, quit)) {
				continue;
			}
			response_error(req, format("%s not yet implemented", req["command"].GetString()).c_str());
		}
	}

	void debugger_impl::update()
	{
		network_->update(0);
		if (is_state(state::birth))
		{
			rprotocol req = network_->input();
			if (req.IsNull()) {
				return;
			}
			if (req["type"] != "request") {
				return;
			}
			if (req["command"] == "initialize") {
				request_initialize(req);
			}
			else if (req["command"] == "disconnect") {
				request_disconnect(req);
			}
		}
		else if (is_state(state::initialized) || is_state(state::running))
		{
			update_redirect();
			rprotocol req = network_->input();
			if (req.IsNull()) {
				return;
			}
			if (req["type"] != "request") {
				return;
			}
			bool quit = false;
			if (!update_main(req, quit)) {
				response_error(req, format("%s not yet implemented", req["command"].GetString()).c_str());
			}
		}
		else if (is_state(state::terminated))
		{
			update_redirect();
			set_state(state::birth);
		}
	}

	void debugger_impl::set_schema(const char* file)
	{
		network_->set_schema(file);
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	debugger_impl::debugger_impl(lua_State* L, const char* ip, uint16_t port)
		: GL(L)
		, seq(1)
		, network_(new network(ip, port))
		, state_(state::birth)
		, step_(step::in)
		, stepping_stacklevel_(0)
		, stepping_lua_state_(NULL)
		, stacklevel_(0)
		, breakpoints_()
		, workingdir_()
		, stack_()
		, watch_(L)
		, redirect_()
		, main_dispatch_
		({
			{ "launch", DBG_REQUEST_MAIN(request_launch) },
			{ "attach", DBG_REQUEST_MAIN(request_attach) },
			{ "disconnect", DBG_REQUEST_MAIN(request_disconnect) },
			{ "setBreakpoints", DBG_REQUEST_MAIN(request_set_breakpoints) },
			{ "configurationDone", DBG_REQUEST_MAIN(request_configuration_done) },
			{ "pause", DBG_REQUEST_MAIN(request_pause) },
		})
		, hook_dispatch_
		({
			{ "continue", DBG_REQUEST_HOOK(request_continue) },
			{ "next", DBG_REQUEST_HOOK(request_next) },
			{ "stepIn", DBG_REQUEST_HOOK(request_stepin) },
			{ "stepOut", DBG_REQUEST_HOOK(request_stepout) },
			{ "stackTrace", DBG_REQUEST_HOOK(request_stack_trace) },
			{ "scopes", DBG_REQUEST_HOOK(request_scopes) },
			{ "variables", DBG_REQUEST_HOOK(request_variables) },
			{ "setVariable", DBG_REQUEST_HOOK(request_set_variable) },
			{ "source", DBG_REQUEST_HOOK(request_source) },
			{ "threads", DBG_REQUEST_HOOK(request_thread) },
			{ "evaluate", DBG_REQUEST_HOOK(request_evaluate) },
		})
	{
	}
#undef DBG_REQUEST_MAIN	 
#undef DBG_REQUEST_HOOK
}
