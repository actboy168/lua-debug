#include "autoattach.h"
#include <bee/nonstd/filesystem.h>
#include <bee/utility/path_helper.h>
#ifndef _WIN32
#include <unistd.h>
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLEXPORT_DECLARATION
#else
#define DLLEXPORT __declspec(dllexport)
#define DLLEXPORT_DECLARATION __cdecl
#endif
#include <string>
#include <atomic>

#include "common.hpp"
#include "../common/common.h"

namespace autoattach {
	static std::string readfile(const fs::path& filename) {
	#ifdef _WIN32
		FILE* f = _wfopen(filename.c_str(), L"rb");
	#else
		FILE* f = fopen(filename.c_str(), "rb");
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

	static void print_error(lua::state L) {
		FAIL_LOG("%s", lua::tostring(L, -1));
		lua::pop(L, 1);
	}

	static void attach(lua::state L) {
		LOG("attach lua vm entry");
		auto r = bee::path_helper::dll_path();
		if (!r) {
			return;
		}
		auto root = lua_debug_common::get_root_path();
		auto buf = readfile(root / "script" / "attach.lua");
		if (lua::loadbuffer(L, buf.data(), buf.size(), "=(attach.lua)")) {
			print_error(L);
			return;
		}
		lua::call<lua_pushstring>(L, root.generic_u8string().c_str());
		
	#ifdef _WIN32
		lua::call<lua_pushstring>(L, std::to_string(GetCurrentProcessId()).c_str());
		void* luaapi = autoattach::luaapi;
		lua::call<lua_pushlstring>(L, (const char*)&luaapi, sizeof(luaapi));
	#else
		lua::call<lua_pushstring>(L, std::to_string(getpid()).c_str());
		lua::call<lua_pushstring>(L, "0");
	#endif
		if (lua::pcall(L, 3, 0, 0)) {
			print_error(L);
		}
	}
}

static void initialize(bool ap) {
	static std::atomic_bool injected;
	bool test = false;
	if (injected.compare_exchange_strong(test, true, std::memory_order_acquire)) {
		LOG("initialize");
		autoattach::initialize(autoattach::attach, ap);
		injected.store(false, std::memory_order_release);
	}
}

extern "C" {
DLLEXPORT void DLLEXPORT_DECLARATION launch() {
	initialize(false);
}

DLLEXPORT void DLLEXPORT_DECLARATION attach() {
	initialize(true);
}
}
