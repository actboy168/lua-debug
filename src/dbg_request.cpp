#include "dbg_impl.h"
#include "dbg_protocol.h"
#include "dbg_network.h"	
#include "dbg_variables.h"	
#include "dbg_format.h"

namespace vscode
{
	fs::path naive_uncomplete(const fs::path& p, const fs::path& base)
	{
		if (p == base)
			return "./";
		fs::path from_path, from_base, output;
		fs::path::iterator path_it = p.begin(), path_end = p.end();
		fs::path::iterator base_it = base.begin(), base_end = base.end();

		if ((path_it == path_end) || (base_it == base_end))
			throw std::runtime_error("path or base was empty; couldn't generate relative path");

#ifdef WIN32
		if (*path_it != *base_it)
			return p;
		++path_it, ++base_it;
#endif

		const std::string _dot = std::string(1, fs::dot<fs::path>::value);
		const std::string _dots = std::string(2, fs::dot<fs::path>::value);
		const std::string _sep = std::string(1, fs::slash<fs::path>::value);

		while (true) {
			if ((path_it == path_end) || (base_it == base_end) || (*path_it != *base_it)) {
				for (; base_it != base_end; ++base_it) {
					if (*base_it == _dot)
						continue;
					else if (*base_it == _sep)
						continue;

					output /= "../";
				}
				fs::path::iterator path_it_start = path_it;
				for (; path_it != path_end; ++path_it) {

					if (path_it != path_it_start)
						output /= "/";

					if (*path_it == _dot)
						continue;
					if (*path_it == _sep)
						continue;

					output /= *path_it;
				}

				break;
			}
			from_path /= fs::path(*path_it);
			from_base /= fs::path(*base_it);
			++path_it, ++base_it;
		}

		return output;
	}

	static void path_normalize(std::string& path)
	{
		std::transform(path.begin(), path.end(), path.begin(),
			[](char c)->char
		{
			if (c == '\\') return '/';
			return tolower((unsigned char)c);
		}
		);
	}

	static std::string get_path(const rapidjson::Value& value)
	{
		assert(value.IsString());
		std::string path(value.GetString(), value.GetStringLength());
		path_normalize(path);
		return path;
	}

	void debugger_impl::set_state(state state)
	{
		state_ = state;
		switch (state_)
		{
		case state::initialized:
			event_initialized();
			open();
			event_output("console", "Debugger initialized\n");
			break;
		case state::terminated:
			event_terminated();
			close();
			break;
		default:
			break;
		}
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
				if (lua_getinfo(L, "S", ar))
				{
					if (ar->source[0] == '@')
					{
						std::string path(ar->source + 1);
						path_normalize(path);
						if (breakpoints_.has(path, ar->currentline))
						{
							step_in();
							return true;
						}
					}
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
		response_initialized(req);
		set_state(state::initialized);
		return false;
	}

	bool debugger_impl::request_launch(rprotocol& req) {
		lua_State *L = GL;
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		auto& args = req["arguments"];
		if (!args.HasMember("program") || !args["program"].IsString()) {
			response_error(req, "Launch failed");
			return false;
		}
		bool stopOnEntry = true;
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
		}
		if (args.HasMember("path") && args["cpath"].IsString())
		{
			std::string path = get_path(args["path"]);
			lua_getglobal(L, "package");
			lua_pushlstring(L, path.data(), path.size());
			lua_setfield(L, -2, "path");
			lua_pop(L, 1);
		}
		if (args.HasMember("cpath") && args["cpath"].IsString())
		{
			std::string path = get_path(args["cpath"]);
			lua_getglobal(L, "package");
			lua_pushlstring(L, path.data(), path.size());
			lua_setfield(L, -2, "cpath");
			lua_pop(L, 1);
		}

		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			workingdir_ = get_path(args["cwd"]);
			fs::current_path(workingdir_);
		}
		std::string program = get_path(args["program"]);
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
		if (lua_pcall(L, 0, 0, 0))
		{
			event_output("console", format("Program terminated with error: %s\n", lua_tostring(L, -1)));
			lua_pop(L, 1);
		}
		set_state(state::terminated);
		return false;
	}

	bool debugger_impl::request_attach(rprotocol& req)
	{
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		auto& args = req["arguments"];
		if (!args.HasMember("program") || !args["program"].IsString()) {
			response_error(req, "Launch failed");
			return false;
		}
		bool stopOnEntry = true;
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
		}

		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			workingdir_ = get_path(args["cwd"]);
			fs::current_path(workingdir_);
		}

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
						if (*src == '@')
						{
							src++;	
							fs::path path(src);
							if (path.is_complete())
							{
								path = naive_uncomplete(path, fs::current_path<fs::path>());
							}
							path = fs::complete(path, workingdir_);
							fs::path name = path.filename();
							for (auto _ : res("source").Object())
							{
								res("name").String(name.string());
								res("path").String(path.string());
								res("sourceReference").Int64(0);
							}
						}
						else if (memcmp(src, "=[C]", 4) == 0)
						{
							for (auto _ : res("source").Object())
							{
								res("name").String("<C function>");
								res("sourceReference").Int64(-1);
							}
						}
						else
						{
							intptr_t reference = ensure_value_fits_in_mantissa((intptr_t)src);
							stack_.push_back({ depth, reference });
							for (auto _ : res("source").Object())
							{
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
		std::string path = get_path(source["path"]);

		breakpoints_.clear(path);

		std::vector<size_t> lines;
		for (auto& m : args["breakpoints"].GetArray())
		{
			size_t line = m["line"].GetUint();
			lines.push_back(line);
			breakpoints_.insert(path, line);
		}

		response_success(req, [&](wprotocol& res)
		{
			for (size_t d : res("breakpoints").Array(lines.size()))
			{
				for (auto _ : res.Object())
				{
					res("verified").Bool(false);
					for (auto _ : res("source").Object())
					{
						res("path").String(path);
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

		response_success(req, [&](wprotocol& res)
		{
			variables resv(res, L, ar);
			resv.push_value(type, depth, var_ref >> 16);
		});
		return false;
	}

	bool debugger_impl::request_configuration_done(rprotocol& req)
	{
		response_success(req);
		return false;
	}

	bool debugger_impl::request_disconnect(rprotocol& req)
	{
		response_success(req);
		set_state(state::terminated);
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
}
