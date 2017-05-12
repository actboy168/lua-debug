#include <lua.hpp>
#include <string>
#include "dbg_impl.h"
#include "dbg_unicode.h"
#include "dbg_format.h"
#include "dbg_io.h"

namespace vscode
{
	int print_empty(lua_State *L) {
		return 0;
	}

	int print(lua_State *L) {
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
		dbg->output("stdout", out.data(), out.size());
		return 0;
	}

	static int errfunc(lua_State* L) {
		debugger_impl* dbg = (debugger_impl*)lua_touserdata(L, lua_upvalueindex(1));
		luaL_traceback(L, L, lua_tostring(L, 1), 1);
		dbg->exception(L, lua_tostring(L, -1));
		lua_settop(L, 2);
		return 1;
	}

	bool debugger_impl::request_launch_done(rprotocol& req) {
		auto& args = req["arguments"];
		if (args.HasMember("runtimeExecutable") && args["runtimeExecutable"].IsString()) {
			return request_attach(req);
		}
		if (launchL_) {
			lua_close(launchL_);
			launchL_ = 0;
		}
		lua_State* L = luaL_newstate();
		luaL_openlibs(L);

		launch_console_ = "none";
		if (args.HasMember("console") && args["console"].IsString())
		{
			launch_console_ = args["console"].Get<std::string>();
		}

		if (launch_console_ == "none") {
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

		bool stopOnEntry = true;
		if (args.HasMember("stopOnEntry") && args["stopOnEntry"].IsBool()) {
			stopOnEntry = args["stopOnEntry"].GetBool();
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
		open_hook(L);
		launchL_ = L;
		return false;
	}

	void debugger_impl::update_launch()
	{
		if (launchL_)
		{
			lua_State *L = launchL_;
			launchL_ = 0;

			lua_pushlightuserdata(L, this);
			lua_pushcclosure(L, errfunc, 1);
			lua_insert(L, -2);
			if (lua_pcall(L, 0, 0, -2))
			{
				event_output("console", format("Program terminated with error: %s\n", lua_tostring(L, -1)));
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
			set_state(state::terminated);
			network_->close();
			lua_close(L);
		}
	}
}
