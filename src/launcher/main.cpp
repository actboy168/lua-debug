#include "autoattach.h"
#include <lua.hpp>
#include <bee/filesystem.h>
#include <bee/utility/path_helper.h>

std::string readfile(const fs::path& filename) {
	FILE* f = _wfopen(filename.c_str(), L"rb");
	if (!f) {
		return std::string();
	}
	fseek (f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string tmp;
	tmp.resize(length);
	fread(tmp.data(), 1, length, f);
	fclose(f);
	return std::move(tmp);
}

static void attach(lua_State* L) {
	auto root = bee::path_helper::dll_path().parent_path().parent_path().parent_path();
	auto buf = readfile(root / "script" / "attach.lua");
	if (luaL_loadbuffer(L, buf.data(), buf.size(), "=(attach.lua)")) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}
	lua_pushstring(L, root.generic_u8string().c_str());
	lua_pushinteger(L, GetCurrentProcessId());
	lua_pushlightuserdata(L, (void*)autoattach::luaapi);
	if (lua_pcall(L, 3, 0, 0)) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static void initialize(bool ap) {
	autoattach::initialize(attach, ap);
}

extern "C" __declspec(dllexport)
void __cdecl launch() {
	initialize(false);
}

extern "C" __declspec(dllexport)
void __cdecl attach() {
	initialize(true);
}
