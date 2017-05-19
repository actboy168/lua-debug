#include "dbg_impl.h"
#include "dbg_protocol.h"  	
#include "dbg_io.h" 
#include "dbg_format.h"
#include <thread>
#include <atomic>
#include <net/datetime/clock.h>

namespace vscode
{
	static custom         global_custom;

	static void debughook(debugger_impl* dbg, lua_State *L, lua_Debug *ar)
	{
		dbg->hook(L, ar);
	}

	void debugger_impl::create_asmjit()
	{
		release_asmjit();
		using namespace asmjit;
		CodeHolder code;
		code.init(asm_jit_.getCodeInfo());
		X86Assembler assemblr(&code);
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(imm(reinterpret_cast<intptr_t>(this)));
		assemblr.call(imm(reinterpret_cast<intptr_t>(&debughook)));
		assemblr.add(x86::esp, 12);
		assemblr.ret();
		Error err = asm_jit_.add(&asm_func_, &code);
		assert(!err);
	}

	void debugger_impl::release_asmjit()
	{
		if (asm_func_)
			asm_jit_.release(asm_func_);
	}

	void debugger_impl::open_hook(lua_State* L)
	{
		stacklevel_.clear();
		if (hookL_ && hookL_ != L) {
			lua_sethook(hookL_, 0, 0, 0);
		}
		lua_sethook(L, asm_func_, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
		hookL_ = L;
	}

	void debugger_impl::close_hook()
	{
		if (hookL_) {
			lua_sethook(hookL_, 0, 0, 0);
			hookL_ = 0;
		}
		breakpoints_.clear();
		stack_.clear();
		seq = 1;
		stacklevel_.clear();
		watch_.reset();
#if !defined(DEBUGGER_DISABLE_LAUNCH)
		update_redirect();
		stderr_.reset();
#endif
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
		if (!allowhook_) { return; }

		std::lock_guard<dbg_thread> lock(*thread_);

		if (ar->event == LUA_HOOKCALL)
		{
			stacklevel_[L]++;
			has_source_ = false;
			return;
		}
		if (ar->event == LUA_HOOKRET)
		{
			if (stacklevel_[L] <= 1) {
				stacklevel_.erase(L);
			}
			else {
				stacklevel_[L]--;
			}
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
				return thread_->update();
			}
			bp = true;
		}

		if (is_state(state::stepping)) {
			if (is_step(step::out) || is_step(step::over)) {
				if (!check_breakpoint(L, ar)) {
					if (!check_step(L, ar)) {
						return thread_->update();
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

		run_stopped(L, ar);
	}

	void debugger_impl::exception(lua_State *L, const char* msg)
	{
		std::lock_guard<dbg_thread> lock(*thread_);

		if (!exception_)
		{
			return;
		}

		allowhook_ = false;
		lua_Debug ar;
		if (lua_getstack(L, 0, &ar))
		{
			event_stopped("exception", msg);
			step_in();
			run_stopped(L, &ar);
		}
		allowhook_ = true;
	}

	void debugger_impl::run_stopped(lua_State *L, lua_Debug *ar)
	{
		bool quit = false;
		while (!quit)
		{
			custom_->update_stop();
#if !defined(DEBUGGER_DISABLE_LAUNCH)
			update_redirect();
#endif
			network_->update(0);

			rprotocol req = network_->input();
			if (req.IsNull()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			if (req["type"] != "request") {
				continue;
			}
			if (is_state(state::birth)) {
				if (req["command"] == "initialize") {
					request_initialize(req);
					continue;
				}
				else if (req["command"] == "disconnect") {
					request_disconnect(req);
					continue;
				}
			}
			else {
				if (update_main(req, quit)) {
					continue;
				}
				if (update_hook(req, L, ar, quit)) {
					continue;
				}
			}
			response_error(req, format("%s not yet implemented", req["command"].GetString()).c_str());
		}
	}

	void debugger_impl::run_idle()
	{
#if !defined(DEBUGGER_DISABLE_LAUNCH)
		update_redirect();
#endif
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
		else if (is_state(state::initialized) || is_state(state::running) || is_state(state::stepping))
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
				return;
			}
		}
		else if (is_state(state::terminated))
		{
			set_state(state::birth);
		}
	}

	void debugger_impl::update()
	{
		{
			std::unique_lock<dbg_thread> lock(*thread_, std::try_to_lock_t());
			if (!lock) {
				return;
			}
			run_idle();
		}

#if !defined(DEBUGGER_DISABLE_LAUNCH)
		update_launch();
#endif
	}

	void debugger_impl::attach_lua(lua_State* L, bool pause)
	{
		attachL_ = L;
		if (pause && L) {
			open_hook(L);
		}
	}

	void debugger_impl::detach_lua(lua_State* L)
	{
		if (attachL_ == L)
		{
			attachL_ = 0;
			set_state(state::terminated);
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
		release_asmjit();
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	struct async
		: public dbg_thread
	{
		async(std::function<void()> const& threadfunc)
			: mtx_()
			, exit_(false)
			, func_(threadfunc)
			, thd_()
		{ }

		~async()
		{
			exit_ = true;
			if (thd_) thd_->join();
		}

		void start() 
		{
			thd_.reset(new std::thread(std::bind(&async::run, this)));
		}

		void run()
		{
			for (
				; !exit_
				; std::this_thread::sleep_for(std::chrono::milliseconds(100)))
			{
				func_();
			}
		}

		void lock() { return mtx_.lock(); }
		bool try_lock() { return mtx_.try_lock(); }
		void unlock() { return mtx_.unlock(); }
		void update() { }

		std::mutex  mtx_;
		std::atomic<bool> exit_;
		std::function<void()> func_;
		std::unique_ptr<std::thread> thd_;
	};

	struct sync
		: public dbg_thread
	{
		sync(std::function<void()> const& threadfunc)
			: func_(threadfunc)
			, time_()
			, last_(time_.now_ms())
		{ }

		void start() {}
		void lock() {}
		bool try_lock() { return true; }
		void unlock() {}
		void update() {
			uint64_t now = time_.now_ms();
			if (now - last_ > 100) {
				now = last_;
				func_();
			}
		}
		std::function<void()>  func_;
		net::datetime::clock_t time_;
		uint64_t               last_;
	};

	debugger_impl::debugger_impl(io* io, threadmode mode)
		: seq(1)
		, network_(io)
		, state_(state::birth)
		, step_(step::in)
		, stepping_stacklevel_(0)
		, stepping_lua_state_(NULL)
		, stacklevel_()
		, breakpoints_()
		, stack_()
		, watch_()
		, pathconvert_(this)
		, custom_(&global_custom)
		, asm_jit_()
		, asm_func_(0)
		, has_source_(false)
		, cur_source_(0)
		, exception_(false)
		, attachL_(0)
		, hookL_(0)
#if !defined(DEBUGGER_DISABLE_LAUNCH)
		, launchL_(0)
		, launch_console_()
#endif
		, allowhook_(true)
		, thread_(mode == threadmode::async 
			? (dbg_thread*)new async(std::bind(&debugger_impl::update, this))
			: (dbg_thread*)new sync(std::bind(&debugger_impl::run_idle, this)))
		, main_dispatch_
		({
#if !defined(DEBUGGER_DISABLE_LAUNCH)
			{ "launch", DBG_REQUEST_MAIN(request_launch) },
			{ "configurationDone", DBG_REQUEST_MAIN(request_configuration_done) },
#endif
			{ "attach", DBG_REQUEST_MAIN(request_attach) },
			{ "disconnect", DBG_REQUEST_MAIN(request_disconnect) },
			{ "setBreakpoints", DBG_REQUEST_MAIN(request_set_breakpoints) },
			{ "setExceptionBreakpoints", DBG_REQUEST_MAIN(request_set_exception_breakpoints) },
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
		create_asmjit();
		thread_->start();
	}

#undef DBG_REQUEST_MAIN	 
#undef DBG_REQUEST_HOOK
}
