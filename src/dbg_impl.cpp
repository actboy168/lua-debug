#include "dbg_impl.h"
#include "dbg_protocol.h"  	
#include "dbg_io.h" 
#include "dbg_format.h"
#include "dbg_thread.h"
#include <thread>
#include <atomic>

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
#if defined(_M_X64)
		assemblr.push(x86::rdi);
		assemblr.sub(x86::rsp, 0x20);
		assemblr.mov(x86::rdi, x86::rsp);
		assemblr.mov(x86::r8, x86::rdx);
		assemblr.mov(x86::rdx, x86::rcx);
		assemblr.mov(x86::rcx, imm(reinterpret_cast<intptr_t>(this)));
		assemblr.call(imm(reinterpret_cast<intptr_t>(&debughook)));
		assemblr.add(x86::rsp, 0x20);
		assemblr.pop(x86::rdi);
#else
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(x86::dword_ptr(x86::esp, 8));
		assemblr.push(imm(reinterpret_cast<intptr_t>(this)));
		assemblr.call(imm(reinterpret_cast<intptr_t>(&debughook)));
		assemblr.add(x86::esp, 12);
#endif
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
		watch_.reset();
		update_redirect();
		stdout_.reset();
		stderr_.reset();
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

	static int print_empty(lua_State *L) {
		return 0;
	}

	static int print(lua_State *L) {
		std::string out;
		int n = lua_gettop(L);
		int i;
		lua_getglobal(L, "tostring");
		for (i = 1; i <= n; i++) {
			const char *s;
			size_t l;
			lua_pushvalue(L, -1);
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			s = lua_tolstring(L, -1, &l);
			if (s == NULL)
				return luaL_error(L, "'tostring' must return a string to 'print'");
			if (i>1) out += "\t";
			out += std::string(s, l);
			lua_pop(L, 1);
		}
		out += "\n";
		debugger_impl* dbg = (debugger_impl*)lua_touserdata(L, lua_upvalueindex(1));
		dbg->output("stdout", out.data(), out.size(), L);
		return 0;
	}


	void debugger_impl::init_redirector(rprotocol& req, lua_State* L) {
		auto& args = req["arguments"];
		console_ = "none";
		if (args.HasMember("console") && args["console"].IsString())
		{
			console_ = args["console"].Get<std::string>();
		}

		if (req.HasMember("__stdout")) {
			if (console_ == "none") {
				lua_pushcclosure(L, print_empty, 0);
				lua_setglobal(L, "print");
			}
			else {
				lua_pushlightuserdata(L, this);
				lua_pushcclosure(L, print, 1);
				lua_setglobal(L, "print");
				stderr_.reset(new redirector);
				stderr_->open("stderr", std_fd::STDERR);
			}
		}
		else {
			if (console_ != "none") {
				stdout_.reset(new redirector);
				stdout_->open("stdout", std_fd::STDOUT);
				stderr_.reset(new redirector);
				stderr_->open("stderr", std_fd::STDERR);
			}
		}
	}
	void debugger_impl::update_redirect()
	{
		if (stdout_) {
			size_t n = stdout_->peek();
			if (n > 0) {
				hybridarray<char, 1024> buf(n);
				stdout_->read(buf.data(), buf.size());
				output("stdout", buf.data(), buf.size(), nullptr);
			}
		}
		if (stderr_) {
			size_t n = stderr_->peek();
			if (n > 0) {
				hybridarray<char, 1024> buf(n);
				stderr_->read(buf.data(), buf.size());
				output("stderr", buf.data(), buf.size(), nullptr);
			}
		}
	}

	static int get_stacklevel(lua_State* L)
	{
		lua_Debug ar;
		int n;
		for (n = 0; lua_getstack(L, n + 1, &ar) != 0; ++n)
		{ }
		return n;
	}

	void debugger_impl::hook(lua_State *L, lua_Debug *ar)
	{
		if (!allowhook_) { return; }

		std::lock_guard<dbg_thread> lock(*thread_);

		if (ar->event == LUA_HOOKCALL)
		{
			has_source_ = false;
			if (stepping_lua_state_ == L) {
				stepping_stacklevel_++;
			}
			return;
		}
		if (ar->event == LUA_HOOKRET)
		{
			has_source_ = false;
			if (stepping_lua_state_ == L) {
				stepping_stacklevel_--;
			}
			return;
		}
		if (ar->event != LUA_HOOKLINE)
			return;
		if (is_state(state::terminated) || is_state(state::birth) || is_state(state::initialized)) {
			return;
		}

		bool bp = check_breakpoint(L, ar);
		if (!bp) {
			if (is_state(state::running)) {
				return thread_->update();
			}
			else if (is_state(state::stepping) && !is_step(step::in) && !check_step(L, ar)) {
				return thread_->update();
			}
			else {
				assert(is_state(state::stepping));
			}
		}

		if (bp)
			event_stopped("breakpoint");
		else
			event_stopped("step");
		step_in();
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
			update_redirect();
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
		update_redirect();
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
				return;
			}
			response_error(req, format("%s not yet implemented", req["command"].GetString()).c_str());
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

			if (thread_->mode() == threadmode::async) {
				semaphore sem;
				attach_callback_ = [&]() {
					sem.signal();
				};
				sem.wait();
				attach_callback_ = std::function<void()>();
			}
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

	void debugger_impl::set_coding(coding coding)
	{
		pathconvert_.set_coding(coding);
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

	void debugger_impl::output(const char* category, const char* buf, size_t len, lua_State* L)
	{
		event_output(category, easy_string(buf, len), L);
	}

	debugger_impl::~debugger_impl()
	{
		release_asmjit();
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	debugger_impl::debugger_impl(io* io, threadmode mode, coding coding)
		: seq(1)
		, network_(io)
		, state_(state::birth)
		, step_(step::in)
		, stepping_stacklevel_(0)
		, stepping_lua_state_(NULL)
		, breakpoints_()
		, stack_()
		, watch_()
		, pathconvert_(this, coding)
		, custom_(&global_custom)
		, asm_jit_()
		, asm_func_(0)
		, has_source_(false)
		, cur_source_(0)
		, exception_(false)
		, attachL_(0)
		, hookL_(0)
		, attach_callback_()
		, console_("none")
#if !defined(DEBUGGER_DISABLE_LAUNCH)
		, launchL_(0)
#endif
		, allowhook_(true)
		, thread_(mode == threadmode::async 
			? (dbg_thread*)new async(std::bind(&debugger_impl::update, this))
			: (dbg_thread*)new sync(std::bind(&debugger_impl::run_idle, this)))
		, main_dispatch_
		({
#if !defined(DEBUGGER_DISABLE_LAUNCH)
			{ "launch", DBG_REQUEST_MAIN(request_launch) },
#endif
			{ "attach", DBG_REQUEST_MAIN(request_attach) },
			{ "configurationDone", DBG_REQUEST_MAIN(request_configuration_done) },
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
