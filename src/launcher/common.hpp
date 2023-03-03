#pragma once
#include <string_view>
#include <bee/nonstd/format.h>

#include "utility/log.h"

typedef struct _RuntimeModule {
    char path[1024];
	char name[256];
    void *load_address;
	size_t size;
    inline bool in_module(void* addr) const {
        return addr >load_address && addr <= (void*)((intptr_t)load_address + size);
    }
} RuntimeModule;

namespace luadebug::autoattach {
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