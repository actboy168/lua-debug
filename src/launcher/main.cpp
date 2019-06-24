#include "autoattach.h"
#include <lua.hpp>
#include <bee/filesystem.h>
#include <bee/utility/path_helper.h>

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

static void attach(lua_State* L) {
    auto root = bee::path_helper::dll_path().value().parent_path().parent_path().parent_path();
    auto buf = file((root / "script" / "attach.lua").c_str()).read<std::string>();
    if (luaL_loadbuffer(L, buf.data(), buf.size(), "=(BOOTSTRAP)")) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
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
