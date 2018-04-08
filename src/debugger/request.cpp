#include <debugger/impl.h>
#include <debugger/protocol.h>
#include <debugger/io/base.h>
#include <base/util/unicode.h>

namespace vscode
{
	void debugger_impl::close()
	{
		set_state(state::terminated);
		detach_all();

		breakpoints_.clear();
		stack_.clear();
		seq = 1;
		ob_.reset();
		update_redirect();
		stdout_.reset();
		stderr_.reset();
	}

	void debugger_impl::set_state(state state)
	{
		if (state_ == state) return;
		state_ = state;
		switch (state_)
		{
		case state::initialized:
			event_initialized();
			break;
		case state::terminated:
			event_terminated();
			break;
		default:
			break;
		}
	}

	bool debugger_impl::is_state(state state) const
	{
		return state_ == state;
	}

	bool debugger_impl::check_breakpoint(lua_State *L, lua::Debug *ar)
	{
		if (ar->currentline > 0 && breakpoints_.has(ar->currentline))
		{
			if (!has_source_)
			{
				has_source_ = true;
				cur_source_ = 0;
				if (!lua_getinfo(L, "S", (lua_Debug*)ar))
					return false;
				cur_source_ = breakpoints_.get(ar->source);
			}
			if (cur_source_ && breakpoints_.has(cur_source_, ar->currentline, L, ar))
			{
				step_in();
				return true;
			}
		}
		return false;
	}

	bool debugger_impl::request_initialize(rprotocol& req) {
		if (!is_state(state::birth)) {
			response_error(req, "already initialized");
			return false;
		}
		response_initialize(req);
		set_state(state::initialized);
		//event_capabilities();
		return false;
	}

	void debugger_impl::initialize_sourcemaps() {
		pathconvert_.clear_sourcemap();
		auto& sourceMaps = config_.get("sourceMaps", rapidjson::kArrayType);
		for (auto& e : sourceMaps.GetArray())
		{
			if (!e.IsArray())
			{
				continue;
			}
			auto& eary = e.GetArray();
			if (eary.Size() < 2 || !eary[0].IsString() || !eary[1].IsString())
			{
				continue;
			}
			pathconvert_.add_sourcemap(eary[0].Get<std::string>(), eary[1].Get<std::string>());
		}
	}

	bool debugger_impl::request_attach(rprotocol& req)
	{
		initproto_ = rprotocol();
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		config_.init(1, req["arguments"]);
		initialize_sourcemaps();
		console_ = config_.get("consoleCoding", rapidjson::kStringType).Get<std::string>();
		auto& sourceCoding = config_.get("sourceCoding", rapidjson::kStringType);
		if (sourceCoding.Get<std::string>() == "utf8") {
			pathconvert_.set_coding(coding::utf8);
		}
		else if (sourceCoding.Get<std::string>() == "ansi") {
			pathconvert_.set_coding(coding::ansi);
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

		event_thread(true);
		if (stopOnEntry)
		{
			set_state(state::stepping);
		}
		else
		{
			set_state(state::running);
		}
		if (on_attach_) {
			on_attach_();
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

	bool debugger_impl::request_thread(rprotocol& req, lua_State* L, lua::Debug *ar) {
		response_thread(req);
		return false;
	}

	bool debugger_impl::request_stack_trace(rprotocol& req, lua_State* L, lua::Debug *ar) {
		response_success(req, [&](wprotocol& res)
		{
			lua::Debug entry;
			auto& args = req["arguments"];
			int depth = 0;
			int levels = args.HasMember("levels") ? args["levels"].GetInt() : 200;
			levels = (levels != 0 ? levels : 200);
			int startFrame = args.HasMember("startFrame") ? args["startFrame"].GetInt() : 0;
			int endFrame = startFrame + levels;
			int curFrame = 0;

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
					if (*entry.what == 'C' && curFrame == 0) {
						depth++;
						continue;
					}
					if (curFrame < startFrame || curFrame >= endFrame) {
						depth++;
						curFrame++;
						continue;
					}
					curFrame++;

					if (*entry.what == 'C') {
						for (auto _ : res.Object()) {
							res("id").Int(depth);
							res("presentationHint").String("label");
							res("name").String(*entry.what == 'm' ? "[main chunk]" : (entry.name ? entry.name : "?"));
							res("line").Int(0);
							res("column").Int(0);
						}
					}
					else if (*entry.source == '@' || *entry.source == '=') {
						for (auto _ : res.Object()) {
							std::string path;
							if (pathconvert_.get(entry.source, path)) {
								for (auto _ : res("source").Object())
								{
									res("name").String(path_filename(path));
									res("path").String(path);
								}
							}
							else {
								res("presentationHint").String("label");
							}
							res("id").Int(depth);
							res("name").String(*entry.what == 'm' ? "[main chunk]" : (entry.name ? entry.name : "?"));
							res("line").Int(entry.currentline);
							res("column").Int(1);
						}
					}
					else {
						intptr_t reference = (intptr_t)entry.source;
						stack_.push_back({ depth, reference });
						for (auto _ : res.Object()) {
							for (auto _ : res("source").Object()) {
								res("name").String("<Memory>");
								res("sourceReference").Int64((intptr_t)entry.source);
							}
							res("id").Int(depth);
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
		int depth = -1;
		for (auto e : stack_) {
			if (e.reference == sourceReference) {
				depth = e.depth;
				break;
			}
		}
		if (lua_getstack(L, depth, (lua_Debug*)&entry)) {
			int status = lua_getinfo(L, "Sln", (lua_Debug*)&entry);
			if (status) {
				const char *src = entry.source;
				if (*src != '@' && *src != '=') {
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
		auto& source = args["source"];
		std::vector<unsigned int> lines;

		if (source.HasMember("path")) {
			std::string path = source["path"].Get<std::string>();
			breakpoints_.clear(path);

			for (auto& m : args["breakpoints"].GetArray())
			{
				unsigned int line = m["line"].GetUint();
				lines.push_back(line);
				breakpoints_.add(path, line, m);
			}
		}
		else if (source.HasMember("name")
			&& source.HasMember("sourceReference"))
		{
			intptr_t source_ref = source["sourceReference"].Get<intptr_t>();
			breakpoints_.clear(source_ref);
			for (auto& m : args["breakpoints"].GetArray())
			{
				unsigned int line = m["line"].GetUint();
				lines.push_back(line);
				breakpoints_.add(source_ref, line, m);
			}
		}
		else {
			response_error(req, "not yet implemented");
			return false;
		}

		response_success(req, [&](wprotocol& res)
		{
			for (size_t d : res("breakpoints").Array(lines.size()))
			{
				for (auto _ : res.Object())
				{
					res("verified").Bool(true);
					res("line").Uint(lines[d]);
				}
			}
		});
		return false;
	}
	
	bool debugger_impl::request_set_exception_breakpoints(rprotocol& req)
	{
		auto& args = req["arguments"];
		exception_ = args.HasMember("filters") && args["filters"].IsArray() && args["filters"].Size() > 0;
		response_success(req);
		return false;
	}

	bool debugger_impl::request_scopes(rprotocol& req, lua_State* L, lua::Debug *ar) {
		ob_.new_frame(L, this, req);
		return false;
	}

	bool debugger_impl::request_variables(rprotocol& req, lua_State* L, lua::Debug *ar) {
		ob_.get_variable(L, this, req);
		return false;
	}

	bool debugger_impl::request_set_variable(rprotocol& req, lua_State *L, lua::Debug *ar) {
		ob_.set_variable(L, this, req);
		return false;
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
		response_success(req);
		step_in();
		return true;
	}

	bool debugger_impl::request_stepout(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		response_success(req);
		step_out(L, ar);
		return true;
	}

	bool debugger_impl::request_next(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		response_success(req);
		step_over(L, ar);
		return true;
	}

	bool debugger_impl::request_continue(rprotocol& req, lua_State* L, lua::Debug *ar)
	{
		response_success(req);
		set_state(state::running);
		return true;
	}

	bool debugger_impl::request_pause(rprotocol& req)
	{
		response_success(req);
		step_in();
		return true;
	}

	bool debugger_impl::request_evaluate(rprotocol& req, lua_State *L, lua::Debug *ar)
	{
		ob_.evaluate(L, ar, this, req);
		return false;
	}

	bool debugger_impl::request_exception_info(rprotocol& req, lua_State *L, lua::Debug *ar)
	{
		response_success(req, [&](wprotocol& res)
		{
			res("breakMode").String("always");
			res("exceptionId").String(lua_tostring(L, -1));
			for (auto _ : res("details").Object())
			{
				luaL_traceback(L, L, 0, 1);
				res("stackTrace").String(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		});
		return false;
	}
}
