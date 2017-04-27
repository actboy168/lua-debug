#include "dbg_impl.h"
#include "dbg_protocol.h"  
#include "dbg_io.h"
#include "dbg_variables.h"	
#include "dbg_format.h"		
#include "dbg_delayload.h"

namespace vscode
{
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
			close();
			break;
		default:
			break;
		}
		custom_->set_state(state);
	}

	bool debugger_impl::is_state(state state)
	{
		return state_ == state;
	}

	void debugger_impl::set_step(step step)
	{
		step_ = step;
	}

	bool debugger_impl::is_step(step step)
	{
		return step_ == step;
	}

	void debugger_impl::step_in()
	{
		set_state(state::stepping);
		set_step(step::in);
		stepping_stacklevel_ = -1000;
		stepping_lua_state_ = 0;
	}

	void debugger_impl::step_over(lua_State* L, lua_Debug* ar)
	{
		set_state(state::stepping);
		set_step(step::over);
		stepping_stacklevel_ = stacklevel_;
		stepping_lua_state_ = L;
	}

	void debugger_impl::step_out(lua_State* L, lua_Debug* ar)
	{
		set_state(state::stepping);
		set_step(step::out);
		stepping_stacklevel_ = stacklevel_ - 1;
		stepping_lua_state_ = L;
	}

	bool debugger_impl::check_step(lua_State* L, lua_Debug* ar)
	{
		return stepping_lua_state_ == L && stepping_stacklevel_ >= stacklevel_;
	}

	bool debugger_impl::check_breakpoint(lua_State *L, lua_Debug *ar)
	{
		if (ar->currentline > 0)
		{
			if (breakpoints_.has(ar->currentline))
			{
				if (!has_source_)
				{
					has_source_ = true;
					cur_source_ = 0;
					if (!lua_getinfo(L, "S", ar))
						return false;
					cur_source_ = breakpoints_.get(ar->source, pathconvert_);
				}

				if (cur_source_ && breakpoints_.has(cur_source_, ar->currentline, L, ar))
				{
					step_in();
					return true;
				}
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
		return false;
	}

	void debugger_impl::initialize_sourcemaps(rapidjson::Value& args) {
		pathconvert_.clear_sourcemap();
		if (!args.HasMember("sourceMaps")) {
			return;
		}
		auto& mapjson = args["sourceMaps"];
		if (!mapjson.IsArray()) {
			return;
		}
		for (auto& e : mapjson.GetArray())
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

	bool debugger_impl::request_launch(rprotocol& req) {
		lua_State *L = GL;
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		auto& args = req["arguments"];
#if defined(DEBUGGER_DELAYLOAD_LUA)	   
		if (args.HasMember("luadll")) {
			delayload::set_lua_dll(vscode::u2w(args["luadll"].Get<std::string>()));
		}
#endif		
		if (!args.HasMember("program") || !args["program"].IsString()) {
			response_error(req, "Launch failed");
			return false;
		}
		initialize_sourcemaps(args);
		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			fs::current_path(fs::path(args["cwd"].Get<std::string>()));
		}
		if (args.HasMember("path") && args["path"].IsString())
		{
			std::string path = u2a(args["path"]);
			lua_getglobal(L, "package");
			lua_pushlstring(L, path.data(), path.size());
			lua_setfield(L, -2, "path");
			lua_pop(L, 1);
		}
		if (args.HasMember("cpath") && args["cpath"].IsString())
		{
			std::string path = u2a(args["cpath"]);
			lua_getglobal(L, "package");
			lua_pushlstring(L, path.data(), path.size());
			lua_setfield(L, -2, "cpath");
			lua_pop(L, 1);
		}
		lua_newtable(L);
		if (args.HasMember("arg0")) {
			if (args["arg0"].IsString()) {
				auto& v = args["arg0"];
				lua_pushlstring(L, v.GetString(), v.GetStringLength());
				lua_rawseti(L, -2, 0);
			}
			else if (args["arg0"].IsArray()) {
				int i = 1 - (int)args["arg0"].Size();
				for (auto& v : args["arg0"].GetArray())
				{
					if (v.IsString()) {
						lua_pushlstring(L, v.GetString(), v.GetStringLength());
					}
					else {
						lua_pushstring(L, "");
					}
					lua_rawseti(L, -2, i++);
				}
			}
		}
		if (args.HasMember("arg") && args["arg"].IsArray()) {
			int i = 1;
			for (auto& v : args["arg"].GetArray())
			{
				if (v.IsString()) {
					lua_pushlstring(L, v.GetString(), v.GetStringLength());
				}
				else {
					lua_pushstring(L, "");
				}
				lua_rawseti(L, -2, i++);
			}
		}
		lua_setglobal(L, "arg");
		cache_launch_ = std::move(req);
		return false;
	}

	static int errfunc(lua_State* L)
	{
		luaL_traceback(L, L, lua_tostring(L, 1), 1);
		return 1;
	}

	bool debugger_impl::request_launch_done(rprotocol& req) {
		lua_State *L = GL;
		bool stopOnEntry = true;
		auto& args = req["arguments"];
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
		}
		std::string program = u2a(args["program"]);
		int status = luaL_loadfile(L, program.c_str());
		if (status != LUA_OK) {
			event_output("console", format("Failed to launch %s due to error: %s\n", program, lua_tostring(L, -1)));
			response_error(req, "Launch failed");
			lua_pop(L, 1);
			return false;
		}
		else
		{
			response_success(req);
		}

		event_thread(true);
		if (stopOnEntry)
		{
			set_state(state::stepping);
			event_stopped("entry");
		}
		else
		{
			set_state(state::running);
		}
		open();

		lua_pushcfunction(L, errfunc);
		lua_insert(L, -2);
		if (lua_pcall(L, 0, 0, -2))
		{
			event_output("console", format("Program terminated with error: %s\n", lua_tostring(L, -1)));
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		set_state(state::terminated);
		network_->close();
		return false;
	}

	bool debugger_impl::request_attach(rprotocol& req)
	{
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		auto& args = req["arguments"];
		bool stopOnEntry = true;
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
		}
		initialize_sourcemaps(args);

		response_success(req);
		event_thread(true);

		if (stopOnEntry)
		{
			set_state(state::stepping);
			event_stopped("entry");
		}
		else
		{
			set_state(state::running);
		}
		open();
		cache_launch_ = rprotocol();
		return !stopOnEntry;
	}

	bool debugger_impl::request_thread(rprotocol& req, lua_State* L, lua_Debug *ar) {
		response_thread(req);
		return false;
	}


	static intptr_t ensure_value_fits_in_mantissa(intptr_t sourceReference) {
		assert(sourceReference <= 9007199254740991);
		return sourceReference;
	}

	bool debugger_impl::request_stack_trace(rprotocol& req, lua_State* L, lua_Debug *ar) {
		response_success(req, [&](wprotocol& res)
		{
			auto& args = req["arguments"];
			int levels = args["levels"].GetInt();
			lua_Debug entry;
			int depth = 0;
			for (auto _ : res("stackFrames").Array())
			{
				while (lua_getstack(L, depth, &entry) && depth < levels)
				{
					for (auto _ : res.Object())
					{
						int status = lua_getinfo(L, "Sln", &entry);
						assert(status);
						const char *src = entry.source;
						if (memcmp(src, "=[C]", 4) == 0)
						{
							for (auto _ : res("source").Object())
							{
								res("name").String("<C function>");
								res("sourceReference").Int64(-1);
								res("presentationHint").String("deemphasize");
							}
						}
						else if (*src == '@' || *src == '=')
						{
							fs::path path;
							custom::result r = pathconvert_.get_or_eval(src, path);
							if (r == custom::result::sucess || r == custom::result::sucess_once)
							{
								fs::path name = path.filename();
								for (auto _ : res("source").Object())
								{
									res("name").String(a2u(name.string()));
									res("path").String(a2u(path.string()));
									res("sourceReference").Int64(0);
								}
							}
						}
						else
						{
							intptr_t reference = ensure_value_fits_in_mantissa((intptr_t)src);
							stack_.push_back({ depth, reference });
							for (auto _ : res("source").Object())
							{
								res("name").String("<Memory funtion>");
								res("sourceReference").Int64(reference);
							}
						}

						res("id").Int(depth);
						res("column").Int(1);
						res("name").String(entry.name ? entry.name : "?");
						res("line").Int(entry.currentline);
						depth++;
					}
				}
			}
			res("totalFrames").Int(depth);
		});
		return false;
	}

	bool debugger_impl::request_source(rprotocol& req, lua_State* L, lua_Debug *ar) {
		auto& args = req["arguments"];
		lua_Debug entry;
		int64_t sourceReference = args["sourceReference"].GetInt64();
		int depth = -1;
		for (auto e : stack_) {
			if (e.reference == sourceReference) {
				depth = e.depth;
				break;
			}
		}
		if (lua_getstack(L, depth, &entry)) {
			int status = lua_getinfo(L, "Sln", &entry);
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
		if (!source.HasMember("path")) {
			response_error(req, "not yet implemented");
			return false;
		}
		fs::path client_path = path_normalize(u2a(source["path"].Get<std::string>()));
		assert(client_path.is_absolute());
		breakpoints_.clear(client_path);

		std::vector<size_t> lines;
		for (auto& m : args["breakpoints"].GetArray())
		{
			size_t line = m["line"].GetUint();
			lines.push_back(line);
			if (!m.HasMember("condition"))
			{
				breakpoints_.insert(client_path, line);
			}
			else
			{
				breakpoints_.insert(client_path, line, m["condition"].Get<std::string>());
			}
		}

		response_success(req, [&](wprotocol& res)
		{
			for (size_t d : res("breakpoints").Array(lines.size()))
			{
				for (auto _ : res.Object())
				{
					res("verified").Bool(true);
					for (auto _ : res("source").Object())
					{
						res("path").String(a2u(client_path.string()));
					}
					res("line").Int(lines[d]);
				}
			}
		});
		return false;
	}

	bool debugger_impl::request_scopes(rprotocol& req, lua_State* L, lua_Debug *ar) {
		auto& args = req["arguments"];
		lua_Debug entry;
		int depth = args["frameId"].GetInt();
		if (!lua_getstack(L, depth, &entry)) {
			response_error(req, "Error retrieving stack frame");
			return false;
		}

		response_success(req, [&](wprotocol& res)
		{
			for (auto _ : res("scopes").Array())
			{
				int status = lua_getinfo(L, "u", &entry);
				assert(status);

				for (auto _ : res.Object())
				{
					res("name").String("Locals");
					res("variablesReference").Int64((int)var_type::local | (depth << 8));
					res("expensive").Bool(false);
				}

				if (entry.isvararg)
				{
					for (auto _ : res.Object())
					{
						res("name").String("Var Args");
						res("variablesReference").Int64((int)var_type::vararg | (depth << 8));
						res("expensive").Bool(false);
					}
				}

				for (auto _ : res.Object())
				{
					res("name").String("Upvalues");
					res("variablesReference").Int64((int)var_type::upvalue | (depth << 8));
					res("expensive").Bool(false);
				}

				for (auto _ : res.Object())
				{
					res("name").String("Globals");
					res("variablesReference").Int64((int)var_type::global | (depth << 8));
					res("expensive").Bool(false);
				}

				for (auto _ : res.Object())
				{
					res("name").String("Standard");
					res("variablesReference").Int64((int)var_type::standard | (depth << 8));
					res("expensive").Bool(false);
				}
			}
		});
		return false;
	}

	bool debugger_impl::request_variables(rprotocol& req, lua_State* L, lua_Debug *ar) {
		auto& args = req["arguments"];
		lua_Debug entry;
		int64_t var_ref = args["variablesReference"].GetInt64();
		var_type type = (var_type)(var_ref & 0xFF);
		int depth = (var_ref >> 8) & 0xFF;
		if (!lua_getstack(L, depth, &entry)) {
			response_error(req, "Error retrieving variables");
			return false;
		}

		if (type == var_type::watch)
		{
			if (!watch_.get((var_ref >> 16) & 0xFF))
			{
				response_error(req, "Error retrieving variables");
			}
		}

		response_success(req, [&](wprotocol& res)
		{
			variables resv(res, L, &entry, type == var_type::watch ? -1 : 0);
			resv.push_value(type, depth, var_ref >> 16, pathconvert_);
		});
		return false;
	}

	bool debugger_impl::request_set_variable(rprotocol& req, lua_State *L, lua_Debug *ar)
	{
		auto& args = req["arguments"];
		lua_Debug entry;
		int64_t var_ref = args["variablesReference"].GetInt64();
		var_type type = (var_type)(var_ref & 0xFF);
		int depth = (var_ref >> 8) & 0xFF;
		if (!lua_getstack(L, depth, &entry)) {
			response_error(req, "Failed set variable");
			return false;
		}

		variable var;
		var.name = args["name"].Get<std::string>();
		var.value = args["value"].Get<std::string>();
		if (!variables::set_value(L, &entry, type, depth, var_ref >> 16, var))
		{
			response_error(req, "Failed set variable");
			return false;
		}
		response_success(req, [&](wprotocol& res)
		{
			res("value").String(var.value);
			res("type").String(var.type);
		});
		return false;
	}

	bool debugger_impl::request_configuration_done(rprotocol& req)
	{
		response_success(req);
		if (cache_launch_.IsNull()) {
			return false;
		}
		return request_launch_done(cache_launch_);
	}

	bool debugger_impl::request_disconnect(rprotocol& req)
	{
		response_success(req);
		set_state(state::terminated);
		network_->close();
		return true;
	}

	bool debugger_impl::request_stepin(rprotocol& req, lua_State* L, lua_Debug *ar)
	{
		response_success(req);
		step_in();
		return true;
	}

	bool debugger_impl::request_stepout(rprotocol& req, lua_State* L, lua_Debug *ar)
	{
		response_success(req);
		step_out(L, ar);
		return true;
	}

	bool debugger_impl::request_next(rprotocol& req, lua_State* L, lua_Debug *ar)
	{
		response_success(req);
		step_over(L, ar);
		return true;
	}

	bool debugger_impl::request_continue(rprotocol& req, lua_State* L, lua_Debug *ar)
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

	bool debugger_impl::request_evaluate(rprotocol& req, lua_State *L, lua_Debug *ar)
	{
		auto& args = req["arguments"];
		auto& context = args["context"];
		std::string expression = args["expression"].Get<std::string>();

		lua_Debug current;
		if (args.HasMember("frameId")) {
			int depth = args["frameId"].GetInt();
			if (!lua_getstack(L, depth, &current)) {
				response_error(req, "error stack frame");
				return false;
			}
		}
		else {
			current = *ar;
		}

		int nresult = 0;
		if (!evaluate(L, &current, ("return " + expression).c_str(), nresult, context == "repl"))
		{
			if (context != "repl")
			{
				response_error(req, lua_tostring(L, -1));
				lua_pop(L, 1);
				return false;
			}
			if (!evaluate(L, &current, expression.c_str(), nresult, true))
			{
				response_error(req, lua_tostring(L, -1));
				lua_pop(L, 1);
				return false;
			}
			response_success(req, [&](wprotocol& res)
			{
				res("result").String("ok");
				res("variablesReference").Int64(0);
			});
			lua_pop(L, nresult);
			return false;
		}
		std::vector<variable> rets(nresult);
		for (int i = 0; i < (int)rets.size(); ++i)
		{
			var_set_value(rets[i], L, i - (int)rets.size(), pathconvert_);
		}
		int64_t reference = 0;
		if (rets.size() == 1 && context == "watch" && can_extand(L, -1))
		{
			size_t pos = watch_.add();
			if (pos > 0)
			{
				reference = (int)var_type::watch | (pos << 16);
			}
		}
		lua_pop(L, nresult);
		response_success(req, [&](wprotocol& res)
		{
			if (rets.size() == 0)
			{
				res("result").String("nil");
			}
			else if (rets.size() == 1)
			{
				res("result").String(rets[0].value);
			}
			else
			{
				std::string result = rets[0].value;
				for (int i = 1; i < (int)rets.size(); ++i)
				{
					result += ", " + rets[i].value;
				}
				res("result").String(result);
			}
			res("variablesReference").Int64(reference);
		});
		return false;
	}
}
