#include "dbg_impl.h"
#include "dbg_protocol.h"  	
#include "dbg_io.h" 
#include "dbg_format.h"
#include <thread>

namespace vscode
{
	static custom         global_custom;

	static void debughook(debugger_impl* dbg, lua_State *L, lua_Debug *ar)
	{
		dbg->hook(L, ar);
	}

	void debugger_impl::open()
	{
		stacklevel_ = 0;

		using namespace asmjit;
		CodeHolder code;
		code.init(jit_.getCodeInfo());

		X86Assembler assemblr(&code);
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(imm(reinterpret_cast<intptr_t>(this)));
		assemblr.call(imm(reinterpret_cast<intptr_t>(&debughook)));
		assemblr.add(x86::esp, 12);
		assemblr.ret();

		lua_Hook hook;
		Error err = jit_.add(&hook, &code);
		assert(!err);
		if (funcptr_)
			jit_.release(funcptr_);
		funcptr_ = hook;
		lua_sethook(GL, hook, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
	}

	void debugger_impl::close()
	{
		lua_sethook(GL, 0, 0, 0);
		breakpoints_.clear();
		stack_.clear();
		seq = 1;
		stacklevel_ = 0;
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
			has_source_ = false;
			return;
		}
		if (ar->event == LUA_HOOKRET)
		{
			stacklevel_--;
			has_source_ = false;
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
			custom_->update_stop();
			network_->update(0);

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
			set_state(state::birth);
		}
	}

	void debugger_impl::set_custom(custom* custom)
	{
		custom_ = custom;
	}

	struct easy_string
	{
		easy_string(const char* buf, size_t len)
			: buf_(buf)
			, len_(len)
		{ }

		const char* data() const
		{
			return buf_;
		}
		size_t size() const
		{
			return len_;
		}

		const char* buf_;
		size_t      len_;
	};

	void debugger_impl::output(const char* category, const char* buf, size_t len)
	{
		event_output(category, easy_string(buf, len));
	}

	debugger_impl::~debugger_impl()
	{
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	debugger_impl::debugger_impl(lua_State* L, io* io)
		: GL(L)
		, seq(1)
		, network_(io)
		, state_(state::birth)
		, step_(step::in)
		, stepping_stacklevel_(0)
		, stepping_lua_state_(NULL)
		, stacklevel_(0)
		, breakpoints_()
		, stack_()
		, watch_(L)
		, pathconvert_(this)
		, custom_(&global_custom)
		, jit_()
		, funcptr_(0)
		, has_source_(false)
		, cur_source_(0)
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
