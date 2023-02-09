#pragma once
#include <string_view>
#include <bee/format.h>

#ifndef NDEBUG
#include <stdio.h>
#define LOG(msg) fprintf(stderr, "[lua-debug][launcher]%s\n", msg)
#else
#define LOG(...)
#endif

typedef struct _RuntimeModule {
    char path[1024];
    void *load_address;
} RuntimeModule;
namespace autoattach {
    enum class lua_version {
        unknown,
        luajit,
        lua51,
        lua52,
        lua53,
        lua54,
		max,
    };

	inline lua_version lua_version_from_string(const std::string_view& v){
		if (v == "luajit")
			return lua_version::luajit;
		if (v == "lua51")
			return lua_version::lua51;
		if (v == "lua52")
			return lua_version::lua52;
		if (v == "lua53")
			return lua_version::lua53;
		if (v == "lua54")
			return lua_version::lua54;
		return lua_version::unknown;
	}
}