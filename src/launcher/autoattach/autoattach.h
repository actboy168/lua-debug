#pragma once

#include <resolver/lua_delayload.h>

namespace luadebug::autoattach {
	struct RuntimeModule {
		char path[1024];
		char name[256];
		void *load_address;
		size_t size;
		inline bool in_module(void* addr) const {
			return addr >load_address && addr <= (void*)((intptr_t)load_address + size);
		}
	};
	typedef int (*fn_attach)(lua::state L);
	void    initialize(fn_attach attach, bool ap);
}
