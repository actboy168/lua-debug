#include <debugger/impl.h>
#include <debugger/protocol.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/path.h>
#include <debugger/osthread.h>
#include <debugger/luathread.h>
#include <base/util/format.h>

namespace vscode
{
	std::string lua_tostr(lua_State* L, int idx)
	{
		size_t len = 0;
		const char* str = luaL_tolstring(L, idx, &len);
		std::string res(str, len);
		lua_pop(L, 1);
		return res;
	}

	static int redirect_print(lua_State* L) {
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

	static lua_State* get_mainthread(lua_State* thread)
	{
		lua_rawgeti(thread, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
		lua_State* ml = lua_tothread(thread, -1);
		lua_pop(thread, 1);
		return ml;
	}

	struct disable_hook {
		disable_hook(lua_State* L)
			: L(L)
			, f(lua_gethook(L))
			, mark(lua_gethookmask(L))
			, count(lua_gethookcount(L))
		{
			lua_sethook(L, 0, 0, 0);
		}
		~disable_hook()
		{
			lua_sethook(L, f, mark, count);
		}
		lua_State* L;
		lua_Hook f;
		int mark;
		int count;
	};

	enum class eCall {
		none,
		pcall,
		xpcall,
	};
	static eCall traceCall(lua_State *L, int level) {
		lua::Debug ar;
		if (LUA_TFUNCTION != lua_getglobal(L, "pcall")) {
			lua_pop(L, 1);
			lua_pushnil(L);
		}
		if (LUA_TFUNCTION != lua_getglobal(L, "xpcall")) {
			lua_pop(L, 1);
			lua_pushnil(L);
		}
		while (lua_getstack(L, level++, (lua_Debug*)&ar)) {
			if (lua_getinfo(L, "f", (lua_Debug*)&ar)) {
				if (lua_rawequal(L, -3, -1)) {
					lua_pop(L, 3);
					return eCall::pcall;
				}
				if (lua_rawequal(L, -2, -1)) {
					lua_pop(L, 3);
					return eCall::xpcall;
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 2);
		return eCall::none;
	}

	luathread* debugger_impl::find_luathread(lua_State* L)
	{
		L = get_mainthread(L);
		for (auto& lt : luathreads_) {
			if (lt.second->L == L) {
				return lt.second.get();
			}
		}
		return nullptr;
	}

	luathread* debugger_impl::find_luathread(int threadid)
	{
		auto it = luathreads_.find(threadid);
		if (it != luathreads_.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	void debugger_impl::attach_lua(lua_State* L)
	{
		if (nodebug_) return;
		luathread* thread = find_luathread(L);
		if (thread) {
			thread->enable_thread();
			return;
		}
		int new_threadid = ++next_threadid_;
		std::unique_ptr<luathread> newthread(new luathread(new_threadid, *this, get_mainthread(L)));
		luathreads_.insert(std::make_pair<int, std::unique_ptr<luathread>>(std::move(new_threadid), std::move(newthread)));
	}

	void debugger_impl::detach_lua(lua_State* L, bool remove)
	{
		luathread* thread = find_luathread(L);
		if (thread) {
			if (remove) {
				luathreads_.erase(thread->id);
			}
			else {
				thread->disable_thread();
			}
		}
	}

	void debugger_impl::detach_all(bool release)
	{
		if (release) {
			for (auto& lt : luathreads_) {
				lt.second->release_thread();
				lt.second->disable_thread();
			}
			luathreads_.clear();
		}
		else {
			for (auto& lt : luathreads_) {
				lt.second->disable_thread();
			}
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

	bool debugger_impl::update_hook(rprotocol& req, lua_State *L, lua::Debug *ar, bool& quit)
	{
		auto it = hook_dispatch_.find(req["command"].Get<std::string>());
		if (it != hook_dispatch_.end())
		{
			quit = it->second(req, L, ar);
			return true;
		}
		return false;
	}

	void debugger_impl::update_redirect()
	{
#if defined(_WIN32)
		if (stdout_) {
			size_t n = stdout_->peek();
			if (n > 0) {
				base::hybrid_array<char, 1024> buf(n);
				stdout_->read(buf.data(), buf.size());
				output("stdout", buf.data(), buf.size());
			}
		}
		if (stderr_) {
			size_t n = stderr_->peek();
			if (n > 0) {
				base::hybrid_array<char, 1024> buf(n);
				stderr_->read(buf.data(), buf.size());
				output("stderr", buf.data(), buf.size());
			}
		}
#endif
	}

	void debugger_impl::panic(luathread* thread, lua_State *L)
	{
		std::lock_guard<osthread> lock(thread_);
		disable_hook db(L);
		exception_nolock(thread, L, eException::lua_panic, 0);
	}

	void debugger_impl::hook(luathread* thread, lua_State *L, lua::Debug *ar)
	{
		std::lock_guard<osthread> lock(thread_);

		if (is_state(eState::terminated) || is_state(eState::birth) || is_state(eState::initialized)) {
			return;
		}

		if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL || ar->event == LUA_HOOKRET) {
			thread->hook_callret(L, ar);
			return;
		}
		if (ar->event == LUA_HOOKEXCEPTION) {
			switch (traceCall(L, 0)) {
			case eCall::pcall:
				exception_nolock(thread, L, eException::pcall, 0);
				break;
			case eCall::xpcall:
				exception_nolock(thread, L, eException::xpcall, 0);
				break;
			case eCall::none:
			default:
				exception_nolock(thread, L, eException::lua_pcall, 0);
				break;
			}
			return;
		}
		if (ar->event != LUA_HOOKLINE) {
			return;
		}
		thread->hook_line(L, ar, breakpoints_);
		if (!thread->cur_function) {
			return;
		}

		if (ar->currentline > 0 && thread->has_breakpoint && breakpoints_.has(thread->cur_function->src, ar->currentline, L, ar)) {
			run_stopped(thread, L, ar, "breakpoint");
		}
		else if (is_state(eState::stepping) && thread->check_step(L, ar)) {
			run_stopped(thread, L, ar, "step");
		}
	}

	void debugger_impl::exception(lua_State* L, eException exceptionType, int level)
	{
		luathread* thread = find_luathread(L);
		if (thread) {
			std::lock_guard<osthread> lock(thread_);
			disable_hook db(L);
			exception_nolock(thread, L, exceptionType, level);
		}
	}

	void debugger_impl::exception_nolock(luathread* thread, lua_State* L, eException exceptionType, int level)
	{
		if (exception_.find(exceptionType) == exception_.end())
		{
			return;
		}

		lua::Debug ar;
		if (lua_getstack(L, 0, (lua_Debug*)&ar))
		{
			lua_pushinteger(L, level);
			if (lua_type(L, -2) == LUA_TSTRING) {
				run_stopped(thread, L, &ar, "exception", lua_tostring(L, -2));
			}
			else {
				run_stopped(thread, L, &ar, "exception");
			}
			lua_pop(L, 1);
		}
	}

	void debugger_impl::run_stopped(luathread* thread, lua_State *L, lua::Debug *ar, const char* reason, const char* description)
	{
		event_stopped(thread, reason, description);
		set_state(eState::stepping);
		thread->step_in();

		bool quit = false;
		while (!quit)
		{
			update_redirect();
			network_->update(0);

			rprotocol req = io_input();
			if (req.IsNull()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			if (req["type"] != "request") {
				continue;
			}
			if (is_state(eState::birth)) {
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
			response_error(req, base::format("`%s` not yet implemented,(stopped)", req["command"].GetString()).c_str());
		}

		thread->reset_session(L);
	}

	void debugger_impl::run_idle()
	{
		update_redirect();
		network_->update(0);
		if (is_state(eState::birth))
		{
			rprotocol req = io_input();
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
			response_error(req, base::format("`%s` not yet implemented.(birth)", req["command"].GetString()).c_str());
		}
		else if (is_state(eState::initialized) || is_state(eState::running) || is_state(eState::stepping))
		{
			rprotocol req = io_input();
			if (req.IsNull()) {
				return;
			}
			if (req["type"] != "request") {
				return;
			}
			bool quit = false;
			if (!update_main(req, quit)) {
				response_error(req, base::format("`%s` not yet implemented.(idle)", req["command"].GetString()).c_str());
				return;
			}
		}
		else if (is_state(eState::terminated))
		{
			set_state(eState::birth);
		}
	}

	void debugger_impl::update()
	{
		std::unique_lock<osthread> lock(thread_, std::try_to_lock_t());
		if (!lock) {
			return;
		}
		run_idle();
	}

	void debugger_impl::wait_client()
	{
		if (!is_state(eState::initialized) && !is_state(eState::birth)) {
			return;
		}
		semaphore sem;
		on_clientattach_ = [&]() {
			sem.signal();
		};
		sem.wait();
		on_clientattach_ = std::function<void()>();
	}

	void debugger_impl::set_custom(custom* custom)
	{
		custom_ = custom;
	}

	void debugger_impl::output(const char* category, const char* buf, size_t len, lua_State* L, lua::Debug* ar)
	{
		if (consoleSourceCoding_ == eCoding::none) {
			return;
		}
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("output");
			for (auto _ : res("body").Object())
			{
				res("category").String(category);

				if (consoleTargetCoding_ == consoleSourceCoding_) {
					res("output").String(base::strview(buf, len));
				}
				else if (consoleSourceCoding_ == eCoding::ansi) {
					res("output").String(base::a2u(base::strview(buf, len)));
				}
				else if (consoleSourceCoding_ == eCoding::utf8) {
					res("output").String(base::u2a(base::strview(buf, len)));
				}

				if (L) {
					lua::Debug entry;
					if (!ar && lua_getstack(L, 1, (lua_Debug*)&entry)) {
						ar = &entry;
					}
					if (!ar) continue;

					int status = lua_getinfo(L, "Sln", (lua_Debug*)ar);
					assert(status);
					if (*ar->what != 'C') {
						source s(ar, *this);
						if (s.valid) {
							s.output(res);
							res("line").Int(ar->currentline);
							res("column").Int(1);
						}
					}
				}
			}
		}
		io_output(res);
	}

	void debugger_impl::open_redirect(eRedirect type, lua_State* L)
	{
		switch (type) {
		case eRedirect::print:
			if (L) {
				lua_pushlightuserdata(L, this);
				lua_pushcclosure(L, redirect_print, 1);
				lua_setglobal(L, "print");
			}
			break;
#if defined(_WIN32)
		case eRedirect::stdoutput:
			stdout_.reset(new redirector);
			stdout_->open("stdout", std_fd::STDOUT);
			break;
		case eRedirect::stderror:
			stderr_.reset(new redirector);
			stderr_->open("stderr", std_fd::STDERR);
			break;
#endif
		}
	}

	bool debugger_impl::set_config(int level, const std::string& cfg, std::string& err)
	{
		return config_.init(level, cfg, err);
	}

	void debugger_impl::io_output(const wprotocol& wp)
	{
		vscode::io_output(network_, wp);
	}

	rprotocol debugger_impl::io_input()
	{
		return vscode::io_input(network_, &schema_);
	}

	void debugger_impl::io_close() 
	{
		network_->update(0);
		network_->close();
	}

	bool debugger_impl::open_schema(const std::string& path)
	{
		return schema_.open(path);
	}

	static void debugger_on_disconnect(void* ud)
	{
		((debugger_impl*)ud)->on_disconnect();
	}

	void debugger_impl::on_disconnect()
	{
		thread_.stop();
		if (thread_.try_lock()) {
			thread_.unlock();
		}
	}

	void debugger_impl::terminate_on_disconnect()
	{
		network_->on_close_event(debugger_on_disconnect, this);
	}

	debugger_impl::~debugger_impl()
	{
		thread_.stop();
		detach_all(true);
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl::name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl::name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	debugger_impl::debugger_impl(io::base* io)
		: seq(1)
		, network_(io)
		, state_(eState::birth)
		, breakpoints_(*this)
		, custom_(nullptr)
		, exception_()
		, luathreads_()
		, on_clientattach_()
		, consoleSourceCoding_(eCoding::none)
		, consoleTargetCoding_(eCoding::utf8)
		, sourceCoding_(eCoding::ansi)
		, workspaceRoot_()
		, nodebug_(false)
		, thread_(this)
		, next_threadid_(0)
		, translator_(nullptr)
		, main_dispatch_
		({
			{ "launch", DBG_REQUEST_MAIN(request_attach) },
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
			{ "threads", DBG_REQUEST_HOOK(request_threads) },
			{ "evaluate", DBG_REQUEST_HOOK(request_evaluate) },
			{ "exceptionInfo", DBG_REQUEST_HOOK(request_exception_info) },
			{ "loadedSources", DBG_REQUEST_HOOK(request_loaded_sources) },
			
		})
	{
		config_.init(2, R"({
			"consoleCoding" : "utf8",
			"sourceCoding" : "ansi"
		})");
		thread_.start();
	}

#undef DBG_REQUEST_MAIN	 
#undef DBG_REQUEST_HOOK
}
