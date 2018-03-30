#include <Windows.h>
#include "launch.h"
#include "stdinput.h"
#include "bridge/dbg_delayload.h"
#include <base/filesystem.h>
#include <base/util/format.h>
#include <base/util/unicode.h>
#include <lua.hpp>

static int errfunc(lua_State* L) {
	vscode::debugger* dbg = (vscode::debugger*)lua_touserdata(L, lua_upvalueindex(1));
	lua_settop(L, 1);
	dbg->exception(L);
	luaL_traceback(L, L, lua_tostring(L, -1), 1);
	return 1;
}

static int print_empty(lua_State* L) {
	return 0;
}

static int print(lua_State* L) {
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
	vscode::debugger* dbg = (vscode::debugger*)lua_touserdata(L, lua_upvalueindex(1));
	dbg->output("stdout", out.data(), out.size(), L);
	return 0;
}

void launch::request_launch(vscode::rprotocol& req) {
	auto& args = req["arguments"];
	if (args.HasMember("luadll")) {
		delayload::set_luadll(base::u2w(args["luadll"].Get<std::string>()));
	}
	bool sourceCodingUtf8 = false;
	std::string sourceCoding = "ansi";
	if (args.HasMember("sourceCoding") && args["sourceCoding"].IsString()) {
		sourceCodingUtf8 = "utf8" == args["sourceCoding"].Get<std::string>();
	}
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		fs::current_path(fs::path(base::u2w(args["cwd"].Get<std::string>())));
	}
	if (args.HasMember("env") && args["env"].IsObject()) {
		for (auto& v : args["env"].GetObject()) {
			if (v.name.IsString()) {
				if (v.value.IsString()) {
					_wputenv((base::u2w(v.name.Get<std::string>()) + L"=" + base::u2w(v.value.Get<std::string>())).c_str());
				}
				else if (v.value.IsNull()) {
					_wputenv((base::u2w(v.name.Get<std::string>()) + L"=").c_str());
				}
			}
		}
	}

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	if (args.HasMember("path") && args["path"].IsString())
	{
		std::string path = sourceCodingUtf8 ? args["path"].Get<std::string>() : base::u2a(args["path"]);
		lua_getglobal(L, "package");
		lua_pushlstring(L, path.data(), path.size());
		lua_setfield(L, -2, "path");
		lua_pop(L, 1);
	}
	if (args.HasMember("cpath") && args["cpath"].IsString())
	{
		std::string path = sourceCodingUtf8 ? args["cpath"].Get<std::string>() : base::u2a(args["cpath"]);
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

	std::string console = "none";
	if (args.HasMember("console") && args["console"].IsString()) {
		console = args["console"].Get<std::string>();
	}
	if (console == "none") {
		lua_pushcclosure(L, print_empty, 0);
		lua_setglobal(L, "print");
	}
	else {
		lua_pushlightuserdata(L, &debugger_);
		lua_pushcclosure(L, print, 1);
		lua_setglobal(L, "print");
	}
	debugger_.redirect_stderr();

	if (launchL_) {
		lua_close(launchL_);
		launchL_ = 0;
	}
	launchL_ = L;
	if (args.HasMember("program") && args["program"].IsString()) {
		program_ = sourceCodingUtf8 ? args["program"].Get<std::string>() : base::u2a(args["program"]);
	}
}

launch::launch(stdinput& io)
	: debugger_(&io, vscode::threadmode::sync)
	, io_(io)
{
}

void launch::update()
{
	debugger_.update();

	if (!launchL_) {
		return;
	}
	if (!debugger_.is_state(vscode::state::running) && !debugger_.is_state(vscode::state::stepping)) {
		return;
	}
	lua_State *L = launchL_;
	launchL_ = 0;

	debugger_.attach_lua(L);

	int status = luaL_loadfile(L, program_.c_str());
	if (status != LUA_OK) {
		auto msg = base::format("Failed to launch `%s` due to error: %s\n", program_, lua_tostring(L, -1));
		debugger_.output("console", msg.data(), msg.size());
		lua_pop(L, 1);
		debugger_.close();
		io_.close();
		lua_close(L);
		return;
	}

	lua_pushlightuserdata(L, &debugger_);
	lua_pushcclosure(L, errfunc, 1);
	lua_insert(L, -2);
	if (lua_pcall(L, 0, 0, -2)) {
		auto msg = base::format("Program terminated with error: %s\n", lua_tostring(L, -1));
		debugger_.output("console", msg.data(), msg.size());
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	debugger_.close();
	io_.close();
	lua_close(L);
}

void launch::send(vscode::rprotocol&& rp)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	rp.Accept(writer);
	io_.push_input(buffer.GetString(), buffer.GetSize());
}
