#pragma once

#include <string_view>
#include <bee/nonstd/format.h>

typedef struct _RuntimeModule {
    char path[1024];
	char name[256];
    void *load_address;
	size_t size;
    inline bool in_module(void* addr) const {
        return addr >load_address && addr <= (void*)((intptr_t)load_address + size);
    }
} RuntimeModule;
