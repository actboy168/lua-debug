#include "autoattach.h"
#include "../remotedebug/rdebug_delayload.h"
#include <lua.hpp>
#include <bee/filesystem.h>
#include <bee/utility/path_helper.h>
#include <atomic>

struct file {
	file(const wchar_t* filename) {
		file_ = _wfopen(filename, L"rb");
	}
	~file() {
		fclose(file_);
	}
	bool is_open() const {
		return !!file_;
	}
	template <class SequenceT>
	SequenceT read() {
		fseek (file_, 0, SEEK_END);
		long length = ftell(file_);
		fseek(file_, 0, SEEK_SET);
		SequenceT tmp;
		tmp.resize(length);
		fread(tmp.data(), 1, length, file_);
		return std::move(tmp);
	}
	FILE* file_;
};

static void initialize(lua_State* L) {
    remotedebug::delayload::set_luadll(autoattach::luadll());
    luaL_openlibs(L);
    auto root = bee::path_helper::dll_path().value().parent_path().parent_path().parent_path();
    auto buf = file((root / "script" / "launcher" / "main.lua").c_str()).read<std::string>();
    if (luaL_loadbuffer(L, buf.data(), buf.size(), "=(BOOTSTRAP)")) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pushstring(L, root.generic_u8string().c_str());
    lua_pushinteger(L, GetCurrentProcessId());
    if (lua_pcall(L, 2, 0, 0)) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static void attach(lua_State* L) {
    static std::atomic<bool> initialized = false;
    bool oldvalue = false;
    if (initialized.compare_exchange_weak(oldvalue, true)) {
        initialize(L);
    }
}

static void detach(lua_State* L) {
    (void)L;
}

static void initialize(bool ap) {
	autoattach::initialize(attach, detach, ap);
}

extern "C" __declspec(dllexport)
void __cdecl launch() {
    initialize(false);
}

extern "C" __declspec(dllexport)
void __cdecl attach() {
    initialize(true);
}
