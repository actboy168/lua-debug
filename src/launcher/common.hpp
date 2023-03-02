#pragma once
#include <string_view>
#include <bee/nonstd/format.h>

#include "utility/log.h"
#ifndef NDEBUG
#include <stdio.h>
#define LOG(msg) fprintf(stderr, "[lua-debug][launcher]%s\n", msg)
#define FATL_LOG(mode, msg) do{LOG(msg); logger::fatal(mode, msg);}while(0)
#else
#define LOG(...) ((void)0)
#define FATL_LOG(mode, msg) logger::fatal(mode, msg)
#endif

typedef struct _RuntimeModule {
    char path[1024];
	char name[256];
    void *load_address;
	size_t size;
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
    lua_version lua_version_from_string(const std::string_view& v);
    const char* lua_version_to_string(lua_version v);
}