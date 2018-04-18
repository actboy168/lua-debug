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
	static lua_State* get_mainthread(lua_State* thread)
	{
		lua_rawgeti(thread, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
		lua_State* ml = lua_tothread(thread, -1);
		lua_pop(thread, 1);
		return ml;
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
		std::unique_ptr<luathread> newthread(new luathread(new_threadid, this, get_mainthread(L)));
		luathreads_.insert(std::make_pair(new_threadid, newthread.release()));
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
	}

	void debugger_impl::hook(luathread* thread, lua_State *L, lua::Debug *ar)
	{
		std::lock_guard<osthread> lock(*thread_);

		if (is_state(state::terminated) || is_state(state::birth) || is_state(state::initialized)) {
			return;
		}

		if (ar->event == LUA_HOOKCALL) {
			thread->hook_call(L, ar);
			return;
		}
		if (ar->event == LUA_HOOKRET) {
			thread->hook_return(L, ar);
			return;
		}
		if (ar->event != LUA_HOOKLINE) {
			return;
		}

		thread->hook_line(L, ar, breakpoints_);
		if ((ar->currentline > 0
			&& thread->cur_bp_
			&& breakpoints_.has(ar->currentline)
			&& breakpoints_.has(thread->cur_bp_, ar->currentline, L, ar)
			)) {
			run_stopped(thread, L, ar, "breakpoint");
		}
		else if (is_state(state::stepping) && thread->check_step(L, ar)) {
			run_stopped(thread, L, ar, "step");
		}
	}

	void debugger_impl::exception(lua_State* L)
	{
		luathread* thread = find_luathread(L);
		if (thread) {
			return exception(thread, L);
		}
	}

	void debugger_impl::exception(luathread* thread, lua_State* L)
	{
		std::lock_guard<osthread> lock(*thread_);

		if (!exception_)
		{
			return;
		}

		lua_Hook f = lua_gethook(L);
		int mark = lua_gethookmask(L);
		int count = lua_gethookcount(L);
		lua_sethook(L, 0, 0, 0);
		lua::Debug ar;
		if (lua_getstack(L, 0, (lua_Debug*)&ar))
		{
			run_stopped(thread, L, &ar, "exception");
		}
		lua_sethook(L, f, mark, count);
	}

	void debugger_impl::run_stopped(luathread* thread, lua_State *L, lua::Debug *ar, const char* reason)
	{
		event_stopped(thread, reason);
		set_state(state::stepping);
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
			response_error(req, base::format("`%s` not yet implemented,(stopped)", req["command"].GetString()).c_str());
		}

		thread->reset_frame(L);
	}

	void debugger_impl::run_idle()
	{
		update_redirect();
		network_->update(0);
		if (is_state(state::birth))
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
		else if (is_state(state::initialized) || is_state(state::running) || is_state(state::stepping))
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
		else if (is_state(state::terminated))
		{
			set_state(state::birth);
		}
	}

	void debugger_impl::update()
	{
		{
			std::unique_lock<osthread> lock(*thread_, std::try_to_lock_t());
			if (!lock) {
				return;
			}
			run_idle();
		}
	}

	void debugger_impl::wait_attach()
	{
		if (!is_state(state::initialized) && !is_state(state::birth)) {
			return;
		}
		semaphore sem;
		on_attach_ = [&]() {
			sem.signal();
		};
		sem.wait();
		on_attach_ = std::function<void()>();
	}

	void debugger_impl::set_custom(custom* custom)
	{
		custom_ = custom;
	}

	void debugger_impl::output(const char* category, const char* buf, size_t len, lua_State* L, lua::Debug* ar)
	{
		if (console_ == "none") {
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
				if (console_ == "ansi") {
					res("output").String(base::a2u(base::strview(buf, len)));
				}
				else {
					res("output").String(base::strview(buf, len));
				}

				if (L) {
					lua::Debug entry;
					if (!ar && lua_getstack(L, 1, (lua_Debug*)&entry)) {
						ar = &entry;
					}
					if (!ar) continue;

					int status = lua_getinfo(L, "Sln", (lua_Debug*)ar);
					assert(status);
					const char *src = ar->source;
					if (*ar->what == 'C')
					{
					}
					else if (*ar->source == '@' || *ar->source == '=')
					{
						std::string path;
						if (pathconvert_.get(src, path))
						{
							for (auto _ : res("source").Object())
							{
								res("name").String(path::filename(path));
								res("path").String(path);
							};
							res("line").Int(ar->currentline);
							res("column").Int(1);
						}
					}
					else
					{
						intptr_t reference = (intptr_t)src;
						for (auto _ : res("source").Object())
						{
							res("name").String("<Memory>");
							res("sourceReference").Int64(reference);
						}
						res("line").Int(ar->currentline);
						res("column").Int(1);
					}
				}
			}
		}
		io_output(res);
	}

	void debugger_impl::redirect_stdout()
	{
		stdout_.reset(new redirector);
		stdout_->open("stdout", std_fd::STDOUT);
	}
		
	void debugger_impl::redirect_stderr()
	{
		stderr_.reset(new redirector);
		stderr_->open("stderr", std_fd::STDERR);
	}

	pathconvert& debugger_impl::get_pathconvert()
	{
		return pathconvert_;
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
		network_->close();
	}

	bool debugger_impl::open_schema(const std::wstring& path)
	{
		return schema_.open(path);
	}

	debugger_impl::~debugger_impl()
	{
		thread_->stop();
		detach_all(true);
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl:: ## name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

	debugger_impl::debugger_impl(io::base* io)
		: seq(1)
		, network_(io)
		, state_(state::birth)
		, breakpoints_(this)
		, stack_()
		, pathconvert_(this)
		, custom_(nullptr)
		, exception_(false)
		, luathreads_()
		, on_attach_()
		, console_("none")
		, nodebug_(false)
		, thread_(new osthread(this))
		, next_threadid_(0)
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
			{ "threads", DBG_REQUEST_HOOK(request_thread) },
			{ "evaluate", DBG_REQUEST_HOOK(request_evaluate) },
			{ "exceptionInfo", DBG_REQUEST_HOOK(request_exception_info) },
		})
	{
		config_.init(2, R"({
			"consoleCoding" : "none",
			"sourceCoding" : "ansi"
		})");
		thread_->start();
	}

#undef DBG_REQUEST_MAIN	 
#undef DBG_REQUEST_HOOK
}
