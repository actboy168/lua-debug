#include "autoattach.h"
#include <lua.hpp>
#include <bee/filesystem.h>
#include <bee/utility/path_helper.h>

#if defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#else
#include <unistd.h>
#define DLLEXPORT __attribute__((visibility("default")))
#endif

std::string readfile(const fs::path& filename) {
#if defined(_WIN32)
	FILE* f = _wfopen(filename.c_str(), L"rb");
#else
	FILE* f = fopen(filename.c_str(), "r");
#endif
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
	return tmp;
}

static void attach(lua_State* L) {
	auto r = bee::path_helper::dll_path();
	if (!r) {
		return;
	}
	auto root = r.value().parent_path().parent_path();
	auto buf = readfile(root / "script" / "attach.lua");
	if (luaL_loadbuffer(L, buf.data(), buf.size(), "=(attach.lua)")) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}
	lua_pushstring(L, root.generic_u8string().c_str());
#if defined(_WIN32)
	lua_pushinteger(L, GetCurrentProcessId());
	lua_pushlightuserdata(L, (void*)autoattach::luaapi);
#else
	lua_pushinteger(L, getpid());
	lua_pushnil(L);
#endif
	if (lua_pcall(L, 3, 0, 0)) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static void initialize(bool ap) {
	autoattach::initialize(attach, ap);
}

extern "C" DLLEXPORT
void launch() {
	initialize(false);
}

extern "C" DLLEXPORT
void attach() {
	initialize(true);
}
