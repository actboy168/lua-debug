#include <debugger/impl.h>
#include <debugger/protocol.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/path.h>
#include <debugger/osthread.h>
#include <debugger/luathread.h>
#include <bee/utility/format.h>
#include <regex>

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
		dbg->threadsafe_output("stdout", out, L);
		return 0;
	}


    static void redirect_write(lua_State* L, int start) {
        debugger_impl* dbg = (debugger_impl*)lua_touserdata(L, lua_upvalueindex(1));
        std::string out;
        int n = lua_gettop(L);
        for (int i = start; i <= n; ++i) {
            if (lua_type(L, i) == LUA_TNUMBER) {
                char s[50];
                if (lua_isinteger(L, i)) {
                    int l = snprintf(s, sizeof(s), LUA_INTEGER_FMT, (LUAI_UACINT)lua_tointeger(L, i));
                    out += std::string(s, l);
                }
                else {
                    int l = snprintf(s, sizeof(s), LUA_NUMBER_FMT, (LUAI_UACNUMBER)lua_tonumber(L, i));
                    out += std::string(s, l);
                }
            }
            else {
                size_t l;
                const char *s = luaL_checklstring(L, i, &l);
                out += std::string(s, l);
            }
        }
        dbg->threadsafe_output("stdout", out, L);
    }

    static int redirect_f_write(lua_State* L) {
        if (LUA_TUSERDATA != lua_getfield(L, LUA_REGISTRYINDEX, "_IO_output") || !lua_rawequal(L, -1, 1)) {
            lua_pop(L, 1);
            lua_pushvalue(L, lua_upvalueindex(2));
            lua_insert(L, 1);
            lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
            return lua_gettop(L);
        }
        lua_pop(L, 1);
        redirect_write(L, 2);
        lua_pushvalue(L, 1);
        return 1;
    }

    static int redirect_io_write(lua_State* L) {
        redirect_write(L, 1);
        lua_getfield(L, LUA_REGISTRYINDEX, "_IO_output");
        return 1;
    }

    static void init_redirect_io_write(lua_State* L, void* dbg) {
        if (LUA_TUSERDATA == lua_getfield(L, LUA_REGISTRYINDEX, "_IO_output")) {
            if (lua_getmetatable(L, -1)) {
                lua_pushstring(L, "write");
                lua_pushlightuserdata(L, dbg);
                lua_pushvalue(L, -2);
                lua_rawget(L, -4);
                lua_pushcclosure(L, redirect_f_write, 2);
                lua_rawset(L, -3);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        if (LUA_TTABLE == lua_getglobal(L, "io")) {
            lua_pushstring(L, "write");
            lua_pushlightuserdata(L, dbg);
            lua_pushcclosure(L, redirect_io_write, 1);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
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

	bool debugger_impl::attach_lua(lua_State* L)
	{
		if (nodebug_) return false;
		luathread* thread = find_luathread(L);
		if (thread) {
			return !thread->enable_thread();
		}
		int new_threadid = ++next_threadid_;
		std::unique_ptr<luathread> newthread(new luathread(new_threadid, *this, get_mainthread(L)));
		luathreads_.insert(std::make_pair<int, std::unique_ptr<luathread>>(std::move(new_threadid), std::move(newthread)));
		return true;
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

	bool debugger_impl::update_hook(rprotocol& req, debug& debug, bool& quit)
	{
		auto it = hook_dispatch_.find(req["command"].Get<std::string>());
		if (it != hook_dispatch_.end())
		{
			quit = it->second(req, debug);
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
				output("stdout", std::string_view(buf.data(), buf.size()));
			}
		}
		if (stderr_) {
			size_t n = stderr_->peek();
			if (n > 0) {
				base::hybrid_array<char, 1024> buf(n);
				stderr_->read(buf.data(), buf.size());
				output("stderr", std::string_view(buf.data(), buf.size()));
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

	struct busy_guard {
		busy_guard(luathread* t) : thread(t) {
			thread->busy = true;
		}
		~busy_guard() {
			thread->busy = false;
		}
		luathread* thread = 0;
	};

	void debugger_impl::hook(luathread* thread, debug& debug)
	{
		std::lock_guard<osthread> lock(thread_);
		busy_guard busy(thread);

		if (is_state(eState::terminated) || is_state(eState::birth) || is_state(eState::initialized)) {
			return;
		}
		
		lua_State* L = debug.L();

		if (debug.event() == LUA_HOOKCALL || debug.event() == LUA_HOOKTAILCALL || debug.event() == LUA_HOOKRET) {
			thread->hook_callret(debug);
			return;
		}
		if (debug.event() == LUA_HOOKEXCEPTION) {
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
		if (debug.event() != LUA_HOOKLINE) {
			return;
		}
		thread->hook_line(debug, breakpointmgr_);
		if (!thread->cur_function) {
			return;
		}

		if (debug.currentline() > 0 && thread->has_breakpoint && breakpointmgr_.has(thread->cur_function, debug.currentline(), debug)) {
			run_stopped(thread, debug, "breakpoint");
		}
		else if (is_state(eState::stepping) && thread->check_step(L)) {
			run_stopped(thread, debug, stopReason_.c_str());
		}
	}

	bool debugger_impl::exception(lua_State* L, eException exceptionType, int level)
	{
		if (!L) {
			for (auto& lt : luathreads_) {
				if (lt.second->busy) {
					return exception(lt.second->L, exceptionType, level);
				}
			}
			return false;
		}

		luathread* thread = find_luathread(L);
		if (thread) {
			std::lock_guard<osthread> lock(thread_);
			disable_hook db(L);
			exception_nolock(thread, L, exceptionType, level);
			return true;
		}
		return false;
	}

	static bool hasFrame(lua_State* L)
	{
		int depth = 0;
		lua::Debug entry;
		while (lua_getstack(L, depth, (lua_Debug*)&entry)) {
			int status = lua_getinfo(L, "S", (lua_Debug*)&entry);
			assert(status);
			if (*entry.what != 'C') {
				return true;
			}
			depth++;
		}
		return false;
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
			if (!hasFrame(L) && lua_type(L, -2) == LUA_TSTRING) {
				run_stopped(thread, debug(L, &ar), "exception", lua_tostring(L, -2));
			}
			else {
				run_stopped(thread, debug(L, &ar), "exception");
			}
			lua_pop(L, 1);
		}
	}

	void debugger_impl::run_stopped(luathread* thread, debug& debug, const char* reason, const char* description)
	{
		event_stopped(thread, reason, description);

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
				if (update_hook(req, debug, quit)) {
					continue;
				}
			}
			response_error(req, bee::format("`%s` not yet implemented,(stopped)", req["command"].GetString()).c_str());
		}

		thread->reset_session(debug.L());
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
			response_error(req, bee::format("`%s` not yet implemented.(birth)", req["command"].GetString()).c_str());
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
				response_error(req, bee::format("`%s` not yet implemented.(idle)", req["command"].GetString()).c_str());
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

	void debugger_impl::threadsafe_output(const char* category, std::string_view text, lua_State* L, lua::Debug* ar)
	{
		std::lock_guard<osthread> lock(thread_);
		return output(category, text, L, ar);
	}

    void debugger_impl::output_raw(const char* category, std::string_view text, source* src, int line)
    {
        if (text.empty()) {
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
                    res("output").String(text);
                }
                else if (consoleSourceCoding_ == eCoding::ansi) {
                    res("output").String(bee::a2u(text));
                }
                else if (consoleSourceCoding_ == eCoding::utf8) {
                    res("output").String(bee::u2a(text));
                }

                if (src && src->valid) {
                    src->output(res);
                    res("line").Int(line);
                }
            }
        }
        io_output(res);
    }

    std::vector<std::string> resplit(const std::string& s, std::string_view pattern) {
        std::vector<std::string> result;
        std::regex re(pattern.data(), pattern.size());
        std::smatch m;
        for (auto& it = s.cbegin(); std::regex_search(it, s.cend(), m, re); it = m[0].second) {
            result.push_back(m.str());
        }
        return result;
    }

    void debugger_impl::stdout_vtmode(std::string& text)
    {
        std::string vt = "";
        if (!stdout_vt_.foreground.empty()) {
            vt += ";";
            vt += stdout_vt_.foreground;
        }
        if (!stdout_vt_.background.empty()) {
            vt += ";";
            vt += stdout_vt_.background;
        }
        if (stdout_vt_.bright) {
            vt += ";1";
        }
        if (stdout_vt_.underline) {
            vt += ";4";
        }
        if (stdout_vt_.negative) {
            vt += ";7";
        }
        try {
            for (auto& vtstr : resplit(text, "\x1b\\[[0-9]+(;[0-9]+)*m")) {
                auto code = resplit(vtstr, "[0-9]+");
                for (size_t n = 0; n < code.size(); ++n) {
                    switch (atoi(code[n].c_str())) {
                    case 0:  // reset
                        stdout_vt_.clear();
                        break;
                    case 1:  // bright
                        stdout_vt_.bright = true;
                        break;
                    case 4:  // underline
                        stdout_vt_.underline = true;
                        break;
                    case 24:  // no underline
                        stdout_vt_.underline = false;
                        break;
                    case 7:  // negative
                        stdout_vt_.negative = true;
                        break;
                    case 27:  // no negative
                        stdout_vt_.negative = false;
                        break;
                    case 30: case 31: case 32: case 33:
                    case 34: case 35: case 36: case 37: // foreground
                    case 90: case 91: case 92: case 93:
                    case 94: case 95: case 96: case 97: // foreground
                        stdout_vt_.foreground = code[n];
                        break;
                    case 40: case 41: case 42: case 43:
                    case 44: case 45: case 46: case 47: // background
                    case 100: case 101: case 102: case 103:
                    case 104: case 105: case 106: case 107: // background
                        stdout_vt_.background = code[n];
                        break;
                    case 39:  // reset foreground
                        stdout_vt_.foreground.clear();
                        break;
                    case 49:  // reset background
                        stdout_vt_.background.clear();
                        break;
                    case 38:  // foreground
                        if (n + 2 < code.size()) {
                            stdout_vt_.foreground = code[n] + ";" + code[n + 1] + ";" + code[n + 2];
                            n += 2;
                        }
                        break;
                    case 48:  // background
                        if (n + 2 < code.size()) {
                            stdout_vt_.background = code[n] + ";" + code[n + 1] + ";" + code[n + 2];
                            n += 2;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        catch (...) {
        }
        if (!vt.empty()) {
            text = "\x1b[" + vt.substr(1) +"m" + text;
        }
    }

    void debugger_impl::output_stdout(std::string_view text, source* src, int line)
    {
        if (text.empty()) {
            return;
        }
        if (stdout_ignore_) {
            assert(stdout_buf_.empty());
            if (text[0] == stdout_ignore_) {
                text.remove_prefix(1);
            }
            stdout_ignore_ = 0;
        }
        stdout_buf_ += text;
        stdout_src_ = src ? src : stdout_src_;
        stdout_line_ = src ? line : stdout_line_;
        for (;;) {
            auto pos = stdout_buf_.find_first_of("\r\n");
            if (pos == std::string::npos) {
                if (src) {
                    stdout_src_ = src;
                    stdout_line_ = line;
                }
                return;
            }
            if (pos + 1 == stdout_buf_.size()) {
                if (stdout_buf_[pos] == '\n') {
                    stdout_ignore_ = '\r';
                }
                else {
                    stdout_ignore_ = '\n';
                }
            }
            else {
                if (stdout_buf_[pos] == '\n' && stdout_buf_[pos + 1] == '\r') {
                    pos++;
                }
                else if (stdout_buf_[pos] == '\r' && stdout_buf_[pos + 1] == '\n') {
                    pos++;
                }
            }
            auto result = stdout_buf_.substr(0, pos + 1);
            stdout_buf_ = stdout_buf_.substr(pos + 1);
            stdout_vtmode(result);
            output_raw("stdout", result, src, line);
            stdout_src_ = src;
            stdout_line_ = line;
        }
    }

    void debugger_impl::output(const char* category, std::string_view text, source* src, int line)
    {
        if (strcmp(category, "stdout") == 0) {
            output_stdout(text, src, line);
        }
        else {
            output_raw(category, text, src, line);
        }
    }

	void debugger_impl::output(const char* category, std::string_view text, lua_State* L, lua::Debug* ar)
	{
        if (consoleSourceCoding_ == eCoding::none) {
            return;
        }
        if (is_state(eState::terminated) || is_state(eState::birth) || is_state(eState::initialized)) {
            return;
        }
        if (L) {
            lua::Debug entry;
            if (!ar && lua_getstack(L, 1, (lua_Debug*)&entry)) {
                ar = &entry;
            }
            if (ar) {
                int status = lua_getinfo(L, "Sln", (lua_Debug*)ar);
                assert(status);
                if (*ar->what != 'C') {
                    source* s = sourcemgr_.create(ar);
                    ar->currentline;
                    output(category, text, s, ar->currentline);
                    return;
                }
            }
        }
        output(category, text, 0, 0);
	}

	void debugger_impl::open_redirect(eRedirect type, lua_State* L)
	{
		switch (type) {
		case eRedirect::print:
			if (L) {
				lua_pushlightuserdata(L, this);
				lua_getglobal(L, "print");
				lua_pushcclosure(L, redirect_print, 2);
				lua_setglobal(L, "print");
				redirectL_ = L;
			}
			break;
        case eRedirect::iowrite:
            init_redirect_io_write(L, this);
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

	void debugger_impl::close_redirect()
	{
		if (redirectL_) {
			lua_State* L = redirectL_;
			redirectL_ = nullptr;
			if (LUA_TFUNCTION == lua_getglobal(L, "print")) {
				if (lua_getupvalue(L, -1, 2)) {
					lua_setglobal(L, "print");
				}
			}
			lua_pop(L, 1);
		}

#if defined(_WIN32)
		stdout_.reset();
		stderr_.reset();
#endif
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
		close();
		if (!attach_) {
			thread_.stop();
			if (thread_.try_lock()) {
				thread_.unlock();
			}
		}
	}

	void debugger_impl::init_internal_module(lua_State* L)
	{
		auto internalModule = config_.get("internalModule", rapidjson::kStringType).Get<std::string>();
		if (internalModule.empty()) {
			return;
		}
		luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
		luaopen_debugger(L);
		lua_setfield(L, -2, internalModule.c_str());
		lua_pop(L, 1);
	}

	void debugger_impl::event(const char* name, lua_State* L, int argf, int argl)
	{
		luathread* thread = find_luathread(L);
		if (!thread) {
			return;
		}
		if (strcmp(name, "call") == 0) {
			source* s = sourcemgr_.createByRef(luaL_checkstring(L, argf));
			if (argf < argl) {
				s->name = luaL_checkstring(L, argf + 1);
			}
			vdebugmgr_.event_call(s);
			thread->dbg.hook(thread, debug::event_call(L));
		}
		else if (strcmp(name, "return") == 0) {
			vdebugmgr_.event_return();
			thread->dbg.hook(thread, debug::event_return(L));
		}
		else if (strcmp(name, "line") == 0) {
			if (argf == argl) {
				thread->dbg.hook(thread, debug::event_line(L, (int)luaL_checkinteger(L, argf), -1));
			}
			else {
				luaL_checktype(L, argf + 1, LUA_TTABLE);
				thread->dbg.hook(thread, debug::event_line(L, (int)luaL_checkinteger(L, argf), lua_absindex(L, argf + 1)));
			}
		}
	}

	source* debugger_impl::createSource(lua::Debug* ar) {
		return sourcemgr_.create(ar);
	}

	bool debugger_impl::getCode(uint32_t ref, std::string& code) {
		return sourcemgr_.getCode(ref, code);
	}

	source* debugger_impl::openVSource() {
		return vdebugmgr_.get_source();
	}

	debugger_impl::~debugger_impl()
	{
		thread_.stop();
		detach_all(true);
	}

#define DBG_REQUEST_MAIN(name) std::bind(&debugger_impl::name, this, std::placeholders::_1)
#define DBG_REQUEST_HOOK(name) std::bind(&debugger_impl::name, this, std::placeholders::_1, std::placeholders::_2)

	debugger_impl::debugger_impl(io::base* io)
		: seq(1)
		, network_(io)
		, state_(eState::birth)
		, breakpointmgr_(*this)
		, sourcemgr_(*this)
		, custom_(nullptr)
		, exception_()
		, luathreads_()
		, on_clientattach_()
		, consoleSourceCoding_(eCoding::none)
		, consoleTargetCoding_(eCoding::utf8)
		, sourceCoding_(eCoding::ansi)
		, workspaceFolder_()
		, nodebug_(false)
		, thread_(this)
		, next_threadid_(0)
		, translator_(nullptr)
		, stopReason_("step")
		, redirectL_(nullptr)
		, attach_(true)
        , stdout_buf_()
        , stdout_src_(0)
        , stdout_line_(0)
        , stdout_ignore_(0)
		, main_dispatch_
		({
			{ "launch", DBG_REQUEST_MAIN(request_launch) },
			{ "attach", DBG_REQUEST_MAIN(request_attach) },
			{ "configurationDone", DBG_REQUEST_MAIN(request_configuration_done) },
			{ "terminate", DBG_REQUEST_MAIN(request_terminate) },
			{ "disconnect", DBG_REQUEST_MAIN(request_disconnect) },
			{ "setBreakpoints", DBG_REQUEST_MAIN(request_set_breakpoints) },
			{ "setExceptionBreakpoints", DBG_REQUEST_MAIN(request_set_exception_breakpoints) },
			{ "pause", DBG_REQUEST_MAIN(request_pause) },
            { "threads", DBG_REQUEST_MAIN(request_threads) },
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
		network_->on_close_event(debugger_on_disconnect, this);
	}

#undef DBG_REQUEST_MAIN	 
#undef DBG_REQUEST_HOOK
}
