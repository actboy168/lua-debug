#include <debugger/impl.h>
#include <debugger/protocol.h>
#include <debugger/path.h>
#include <debugger/luathread.h>
#include <debugger/io/base.h>
#include <base/util/unicode.h>

namespace vscode
{
	void closeprocess();

	void debugger_impl::close()
	{
		if (is_state(eState::terminated) || is_state(eState::birth)) {
			return;
		}

		update_redirect();
#if defined(_WIN32)
		stdout_.reset();
		stderr_.reset();
#endif
		set_state(eState::terminated);
		event_terminated();
		detach_all(false);

		breakpoints_.clear();
		seq = 1;
	}

	void debugger_impl::set_state(eState state)
	{
		if (state_ == state) return;
		state_ = state;
	}

	bool debugger_impl::is_state(eState state) const
	{
		return state_ == state;
	}

	bool debugger_impl::request_initialize(rprotocol& req) {
		if (!is_state(eState::birth)) {
			response_error(req, "already initialized");
			return false;
		}
		auto& args = req["arguments"];
		if (args.HasMember("locale") && args["locale"].IsString()) {
			setlang(args["locale"].Get <std::string>());
		}
		response_initialize(req);
		set_state(eState::initialized);
		event_initialized();
		event_capabilities();
		return false;
	}

	bool debugger_impl::request_attach(rprotocol& req)
	{
		initproto_ = rprotocol();
		if (!is_state(eState::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		config_.init(1, req["arguments"]);
		initialize_pathconvert(config_);
		auto consoleCoding = config_.get("consoleCoding", rapidjson::kStringType).Get<std::string>();
		if (consoleCoding == "ansi") {
			consoleSourceCoding_ = eCoding::ansi;
		}
		else if(consoleCoding == "utf8") {
			consoleSourceCoding_ = eCoding::utf8;
		}
		auto& sourceCoding = config_.get("sourceCoding", rapidjson::kStringType);
		if (sourceCoding.Get<std::string>() == "utf8") {
			sourceCoding_ = eCoding::utf8;
		}
		else if (sourceCoding.Get<std::string>() == "ansi") {
			sourceCoding_ = eCoding::ansi;
		}

		nodebug_ = config_.get("noDebug", rapidjson::kFalseType).GetBool();
		response_success(req);
		initproto_ = std::move(req);
		return false;
	}

	bool debugger_impl::request_attach_done(rprotocol& req)
	{
		bool stopOnEntry = true;
		auto& args = req["arguments"];
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
		}

		if (stopOnEntry) {
			set_state(eState::stepping);
		}
		else {
			set_state(eState::running);
		}
		if (on_clientattach_) {
			on_clientattach_();
		}
		return !stopOnEntry;
	}

	bool debugger_impl::request_configuration_done(rprotocol& req) {
		response_success(req);
		if (initproto_.IsNull()) {
			return false;
		}
		return request_attach_done(initproto_);
	}

	bool debugger_impl::request_threads(rprotocol& req, lua_State* L, lua::Debug *ar) {
		response_threads(req);
		return false;
	}

	bool debugger_impl::request_stack_trace(rprotocol& req, lua_State* L, lua::Debug *ar) {
		auto& args = req["arguments"];
		if (!args.HasMember("threadId") || !args["threadId"].IsInt()) {
			response_error(req, "Not found thread");
			return false;
		}
		int threadId = args["threadId"].GetInt();
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}

		int depth = 0;
		int levels = args.HasMember("levels") ? args["levels"].GetInt() : 200;
		levels = (levels != 0 ? levels : 200);
		int startFrame = args.HasMember("startFrame") ? args["startFrame"].GetInt() : 0;
		int endFrame = startFrame + levels;
		int curFrame = 0;

		response_success(req, [&](wprotocol& res)
		{
			lua::Debug entry;
			for (auto _ : res("stackFrames").Array())
			{
				while (lua_getstack(L, depth, (lua_Debug*)&entry))
				{
					if (curFrame != 0 && (curFrame < startFrame || curFrame >= endFrame)) {
						depth++;
						curFrame++;
						continue;
					}
					int status = lua_getinfo(L, "Sln", (lua_Debug*)&entry);
					assert(status);
					if (curFrame == 0) {
						if (*entry.what == 'C') {
							depth++;
							continue;
						}
						else {
							source s(&entry, *this);
							if (!s.valid) {
								depth++;
								continue;
							}
						}
					}
					if (curFrame < startFrame || curFrame >= endFrame) {
						depth++;
						curFrame++;
						continue;
					}
					curFrame++;

					if (*entry.what == 'C') {
						for (auto _ : res.Object()) {
							res("id").Int(threadId << 16 | depth);
							res("presentationHint").String("label");
							res("name").String(*entry.what == 'm' ? "[main chunk]" : (entry.name ? entry.name : "?"));
							res("line").Int(0);
							res("column").Int(0);
						}
					}
					else {
						for (auto _ : res.Object()) {
							source s(&entry, *this);
							if (s.valid) {
								s.output(res);
							}
							else {
								res("presentationHint").String("label");
							}
							res("id").Int(threadId << 16 | depth);
							res("name").String(*entry.what == 'm' ? "[main chunk]" : (entry.name ? entry.name : "?"));
							res("line").Int(entry.currentline);
							res("column").Int(1);
						}
					}
					depth++;
				}
			}
			res("totalFrames").Int(curFrame);
		});
		return false;
	}

	bool debugger_impl::request_source(rprotocol& req, lua_State* L, lua::Debug *ar) {
		auto& args = req["arguments"];
		lua::Debug entry;
		int64_t sourceReference = args["sourceReference"].GetInt64();
		for (int depth = 0;; ++depth) {
			if (!lua_getstack(L, depth, (lua_Debug*)&entry)) {
				break;
			}
			int status = lua_getinfo(L, "S", (lua_Debug*)&entry);
			if (status) {
				const char *src = entry.source;
				if ((int64_t)src == sourceReference) {
					response_source(req, src);
					return false;
				}
			}
		}
		response_source(req, "Source not available");
		return false;
	}

	bool debugger_impl::request_set_breakpoints(rprotocol& req)
	{
		auto& args = req["arguments"];
		vscode::source s(args["source"]);
		if (!s.valid) {
			response_error(req, "not yet implemented");
			return false;
		}
		for (auto& lt : luathreads_) {
			lt.second->update_breakpoint();
		}
		response_success(req, [&](wprotocol& res)
		{
			breakpoints_.set_breakpoint(s, args, res);
		});
		return false;
	}

	bool debugger_impl::request_set_exception_breakpoints(rprotocol& req)
	{
		auto& args = req["arguments"];
		exception_.clear();
		if (args.HasMember("filters") && args["filters"].IsArray()) {
			for (auto& v : args["filters"].GetArray()) {
				if (v.IsString()) {
					std::string filter = v.Get<std::string>();
					if (filter == "all") {
						exception_.insert(eException::lua_panic);
						exception_.insert(eException::lua_pcall);
						exception_.insert(eException::xpcall);
						exception_.insert(eException::pcall);
					}
					else if (filter == "lua_panic") {
						exception_.insert(eException::lua_panic);
					}
					else if (filter == "lua_pcall") {
						exception_.insert(eException::lua_pcall);
					}
					else if (filter == "pcall") {
						exception_.insert(eException::pcall);
					}
					else if (filter == "xpcall") {
						exception_.insert(eException::xpcall);
					}
				}
			}

		}
		response_success(req);
		return false;
	}

	bool debugger_impl::request_scopes(rprotocol& req, lua_State* L, lua::Debug *ar) {
		auto& args = req["arguments"];
		if (!args.HasMember("frameId")) {
			response_error(req, "Not found frame");
			return false;
		}
		int threadAndFrameId = args["frameId"].GetInt();
		int threadId = threadAndFrameId >> 16;
		int frameId = threadAndFrameId & 0xFFFF;
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		thread->new_frame(L, *this, req, frameId);
		return false;
	}

	bool debugger_impl::request_variables(rprotocol& req, lua_State* L, lua::Debug *ar) {
		auto& args = req["arguments"];
		int64_t valueId = args["variablesReference"].GetInt64();
		int threadId = valueId >> 32;
		int frameId = (valueId >> 16) & 0xFFFF;
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		thread->get_variable(L, *this, req, valueId, frameId);
		return false;
	}

	bool debugger_impl::request_set_variable(rprotocol& req, lua_State *L, lua::Debug *ar) {
		auto& args = req["arguments"];
		int64_t valueId = args["variablesReference"].GetInt64();
		int threadId = valueId >> 32;
		int frameId = (valueId >> 16) & 0xFFFF;
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		thread->set_variable(L, *this, req, valueId, frameId);
		return false;
	}

	bool debugger_impl::request_terminate(rprotocol& req)
	{
		response_success(req);
		closeprocess();
		return true;
	}

	bool debugger_impl::request_disconnect(rprotocol& req)
	{
		response_success(req);
		close();
		io_close();
		return true;
	}

	bool debugger_impl::request_stepin(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		auto& args = req["arguments"];
		if (!args.HasMember("threadId") || !args["threadId"].IsInt()) {
			response_error(req, "Not found thread");
			return false;
		}
		int threadId = args["threadId"].GetInt();
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		set_state(eState::stepping);
		thread->step_in();
		response_success(req);
		return true;
	}

	bool debugger_impl::request_stepout(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		auto& args = req["arguments"];
		if (!args.HasMember("threadId") || !args["threadId"].IsInt()) {
			response_error(req, "Not found thread");
			return false;
		}
		int threadId = args["threadId"].GetInt();
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		set_state(eState::stepping);
		thread->step_out(L, ar);
		response_success(req);
		return true;
	}

	bool debugger_impl::request_next(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		auto& args = req["arguments"];
		if (!args.HasMember("threadId") || !args["threadId"].IsInt()) {
			response_error(req, "Not found thread");
			return false;
		}
		int threadId = args["threadId"].GetInt();
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		set_state(eState::stepping);
		thread->step_over(L, ar);
		response_success(req);
		return true;
	}

	bool debugger_impl::request_continue(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		response_success(req);
		set_state(eState::running);
		return true;
	}

	bool debugger_impl::request_pause(rprotocol& req)
	{
		auto& args = req["arguments"];
		if (!args.HasMember("threadId") || !args["threadId"].IsInt()) {
			response_error(req, "Not found thread");
			return false;
		}
		int threadId = args["threadId"].GetInt();
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		set_state(eState::stepping);
		thread->step_in();
		response_success(req);
		return true;
	}

	bool debugger_impl::request_evaluate(rprotocol& req, lua_State *L, lua::Debug *ar)
	{
		auto& args = req["arguments"];
		if (!args.HasMember("frameId")) {
			response_error(req, "Not yet implemented.");
			return false;
		}
		int threadAndFrameId = args["frameId"].GetInt();
		int threadId = threadAndFrameId >> 16;
		int frameId = threadAndFrameId & 0xFFFF;
		luathread* thread = find_luathread(threadId);
		if (!thread) {
			response_error(req, "Not found thread");
			return false;
		}
		lua::Debug current;
		if (!lua_getstack(L, frameId, (lua_Debug*)&current)) {
			response_error(req, "Error frame");
			return false;
		}
		thread->evaluate(L, &current, *this, req, frameId);
		return false;
	}

	std::string safe_tostr(lua_State* L, int idx)
	{
		if (lua_type(L, idx) == LUA_TSTRING) {
			size_t len = 0;
			const char* str = lua_tolstring(L, idx, &len);
			return std::string(str, len);
		}
		return std::string(lua_typename(L, lua_type(L, idx)));
	}

	bool debugger_impl::request_exception_info(rprotocol& req, lua_State *L, lua::Debug *ar)
	{
		//TODO: threadId
		response_success(req, [&](wprotocol& res)
		{
			res("breakMode").String("always");
			std::string exceptionId = safe_tostr(L, -2);
			luaL_traceback(L, L, 0, (int)lua_tointeger(L, -1));
			std::string stackTrace = safe_tostr(L, -1);
			lua_pop(L, 1);

			exceptionId = path_exception(exceptionId);
			stackTrace = path_exception(stackTrace);

			res("exceptionId").String(exceptionId);
			for (auto _ : res("details").Object())
			{
				res("stackTrace").String(stackTrace);
			}
		});
		return false;
	}

	bool debugger_impl::request_loaded_sources(rprotocol& req, lua_State *L, lua::Debug *ar)
	{
		response_success(req, [&](wprotocol& res)
		{
			breakpoints_.loaded_sources(res);
		});
		return false;
	}
}
