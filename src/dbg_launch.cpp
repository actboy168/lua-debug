#if !defined(DEBUGGER_DISABLE_LAUNCH)

#include <Windows.h>
#include <string>
#include "dbg_impl.h"
#include "dbg_unicode.h"
#include "dbg_format.h"
#include "dbg_io.h"
#include "dbg_delayload.h"

namespace vscode
{
	static int errfunc(lua_State* L) {
		debugger_impl* dbg = (debugger_impl*)lua_touserdata(L, lua_upvalueindex(1));
		luaL_traceback(L, L, lua_tostring(L, 1), 1);
		dbg->exception(L, lua_tostring(L, -1));
		lua_settop(L, 2);
		return 1;
	}

	bool debugger_impl::request_launch(rprotocol& req) {
		initproto_ = rprotocol();
		if (!is_state(state::initialized)) {
			response_error(req, "not initialized or unexpected state");
			return false;
		}
		auto& args = req["arguments"];
		if (args.HasMember("runtimeExecutable") && args["runtimeExecutable"].IsString()) {
			initproto_ = std::move(req);
			return false;
		}
		if (!args.HasMember("program") || !args["program"].IsString()) {
			response_error(req, "Launch failed");
			return false;
		}
#if defined(DEBUGGER_DELAYLOAD_LUA)	   
		if (args.HasMember("luadll")) {
			delayload::set_luadll(vscode::u2w(args["luadll"].Get<std::string>()));
		}
#endif		
		initialize_sourcemaps(args);
		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			fs::current_path(fs::path(u2w(args["cwd"].Get<std::string>())));
		}
		if (args.HasMember("env") && args["env"].IsObject()) {
			for (auto& v : args["env"].GetObject()) {
				if (v.name.IsString()) {
					if (v.value.IsString()) {
						_wputenv((vscode::u2w(v.name.Get<std::string>()) + L"=" + vscode::u2w(v.value.Get<std::string>())).c_str());
					}
					else if (v.value.IsNull()) {
						_wputenv((vscode::u2w(v.name.Get<std::string>()) + L"=").c_str());
					}
				}
			}
		}
		initproto_ = std::move(req);
		return false;
	}

	bool debugger_impl::request_launch_done(rprotocol& req) {
		auto& args = req["arguments"];
		if (args.HasMember("runtimeExecutable") && args["runtimeExecutable"].IsString()) {
			rprotocol tmp = std::move(req);
			request_attach(tmp);
			return request_attach_done(initproto_);
		}
		if (launchL_) {
			lua_close(launchL_);
			launchL_ = 0;
		}
		lua_State* L = luaL_newstate();
		luaL_openlibs(L);
		init_redirector(req, L);

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
		launchL_ = L;
		return false;
	}

	void debugger_impl::update_launch()
	{
		if (launchL_)
		{
			lua_State *L = launchL_;
			launchL_ = 0;

			attach_lua(L);
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

#endif
