#include <debugger/impl.h>
#include <debugger/protocol.h>
#include <debugger/io/base.h>
#include <debugger/thread.h>
#include <debugger/thunk.h>
#include <debugger/path.h>
#include <debugger/io/helper.h>
#include <base/util/format.h>
#include <thread>
#include <atomic>

namespace vscode
{
	static lua_State* get_mainthread(lua_State* thread)
	{
		lua_rawgeti(thread, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
		lua_State* ml = lua_tothread(thread, -1);
		lua_pop(thread, 1);
		return ml;
	}

	static int get_stacklevel(lua_State* L)
	{
		lua::Debug ar;
		int n;
		for (n = 0; lua_getstack(L, n + 1, (lua_Debug*)&ar) != 0; ++n)
		{
		}
		return n;
	}

	static int get_stacklevel(lua_State* L, int pos)
	{
		lua::Debug ar;
		if (lua_getstack(L, pos, (lua_Debug*)&ar) != 0) {
			for (; lua_getstack(L, pos + 1, (lua_Debug*)&ar) != 0; ++pos)
			{
			}
		}
		else if (pos > 0) {
			for (--pos; pos > 0 && lua_getstack(L, pos, (lua_Debug*)&ar) == 0; --pos)
			{
			}
		}
		return pos;
	}

	static void debugger_hook(debugger_impl::lua_thread* thread, lua_State *L, lua::Debug *ar)
	{
		thread->dbg->hook(thread, L, ar);
	}

	static void debugger_panic(debugger_impl::lua_thread* thread, lua_State *L)
	{
		thread->dbg->exception(thread, L);
	}

	debugger_impl::lua_thread::lua_thread(int id, debugger_impl* dbg, lua_State* L)
		: id(id)
		, dbg(dbg)
		, L(L)
		, oldpanic(lua_atpanic(L, 0))
		, thunk_hook((lua_Hook)thunk_create_hook(
			reinterpret_cast<intptr_t>(this),
			reinterpret_cast<intptr_t>(&debugger_hook)
		))
		, thunk_panic((lua_CFunction)thunk_create_panic(
			reinterpret_cast<intptr_t>(this),
			reinterpret_cast<intptr_t>(&debugger_panic),
			reinterpret_cast<intptr_t>(oldpanic)
		))
		, step_(step::in)
		, stepping_target_level_(0)
		, stepping_current_level_(0)
		, stepping_lua_state_(NULL)
		, has_source_(false)
		, cur_source_(0)
	{
		lua_sethook(L, thunk_hook, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
		lua_atpanic(L, thunk_panic);;
	}

	debugger_impl::lua_thread::~lua_thread()
	{
		lua_sethook(L, 0, 0, 0);
		lua_atpanic(L, oldpanic);
		thunk_destory(thunk_hook);
		thunk_destory(thunk_panic);
	}

	void debugger_impl::lua_thread::set_step(step step)
	{
		step_ = step;
	}

	bool debugger_impl::lua_thread::is_step(step step)
	{
		return step_ == step;
	}

	bool debugger_impl::lua_thread::check_step(lua_State* L, lua::Debug* ar)
	{
		return stepping_lua_state_ == L && stepping_current_level_ <= stepping_target_level_;
	}

	void debugger_impl::lua_thread::step_in()
	{
		set_step(step::in);
	}

	void debugger_impl::lua_thread::step_over(lua_State* L, lua::Debug* ar)
	{
		set_step(step::over);
		stepping_target_level_ = stepping_current_level_ = get_stacklevel(L);
		stepping_lua_state_ = L;
	}

	void debugger_impl::lua_thread::step_out(lua_State* L, lua::Debug* ar)
	{
		set_step(step::out);
		stepping_target_level_ = stepping_current_level_ = get_stacklevel(L);
		stepping_target_level_--;
		stepping_lua_state_ = L;
	}

	void debugger_impl::lua_thread::hook_call(lua_State* L, lua::Debug* ar)
	{
		has_source_ = false;
		if (stepping_lua_state_ == L) {
			stepping_current_level_++;
		}
	}

	void debugger_impl::lua_thread::hook_return(lua_State* L, lua::Debug* ar)
	{
		has_source_ = false;
		if (stepping_lua_state_ == L) {
			stepping_current_level_ = get_stacklevel(L, stepping_current_level_) - 1;
		}
	}

	void debugger_impl::lua_thread::reset_frame(lua_State* L)
	{
		ob_.reset(L);
	}

	void debugger_impl::lua_thread::evaluate(lua_State* L, lua::Debug *ar, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		ob_.evaluate(L, ar, dbg, req, frameId);
	}

	void debugger_impl::lua_thread::new_frame(lua_State* L, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		ob_.new_frame(L, dbg, req, frameId);
	}

	void debugger_impl::lua_thread::get_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		ob_.get_variable(L, dbg, req, valueId, frameId);
	}

	void debugger_impl::lua_thread::set_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		ob_.set_variable(L, dbg, req, valueId, frameId);
	}

	debugger_impl::lua_thread* debugger_impl::find_luathread(lua_State* L)
	{
		L = get_mainthread(L);
		for (auto& lt : luathreads_) {
			if (lt.second->L == L) {
				return lt.second.get();
			}
		}
		return nullptr;
	}

	debugger_impl::lua_thread* debugger_impl::find_luathread(int threadid)
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
		if (find_luathread(L)) return;
		std::unique_ptr<lua_thread> thread(new lua_thread(++threadid_, this, get_mainthread(L)));
		luathreads_.insert(std::make_pair(thread->id, thread.release()));
	}

	void debugger_impl::detach_lua(lua_State* L)
	{
		lua_thread* thread = find_luathread(L);
		if (thread) {
			luathreads_.erase(thread->id);
		}
	}

	void debugger_impl::detach_all()
	{
		luathreads_.clear();
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

	void debugger_impl::hook(lua_thread* thread, lua_State *L, lua::Debug *ar)
	{
		std::lock_guard<dbg_thread> lock(*thread_);

		if (ar->event == LUA_HOOKCALL)
		{
			thread->hook_call(L, ar);
			return;
		}
		if (ar->event == LUA_HOOKRET)
		{
			thread->hook_return(L, ar);
			return;
		}
		if (ar->event != LUA_HOOKLINE)
			return;
		if (is_state(state::terminated) || is_state(state::birth) || is_state(state::initialized)) {
			return;
		}

		bool bp = check_breakpoint(thread, L, ar);
		if (!bp) {
			if (is_state(state::running)) {
				return;
			}
			else if (is_state(state::stepping) && !thread->is_step(step::in) && !thread->check_step(L, ar)) {
				return;
			}
			else {
				assert(is_state(state::stepping));
			}
		}

		run_stopped(thread, L, ar, bp? "breakpoint": "step");
	}

	void debugger_impl::exception(lua_State* L)
	{
		lua_thread* thread = find_luathread(L);
		if (thread) {
			return exception(thread, L);
		}
	}

	void debugger_impl::exception(lua_thread* thread, lua_State* L)
	{
		std::lock_guard<dbg_thread> lock(*thread_);

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

	void debugger_impl::run_stopped(lua_thread* thread, lua_State *L, lua::Debug *ar, const char* reason)
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
			std::unique_lock<dbg_thread> lock(*thread_, std::try_to_lock_t());
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
		detach_all();
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
		, thread_(new dbg_thread(this))
		, threadid_(0)
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
