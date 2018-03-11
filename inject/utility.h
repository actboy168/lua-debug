#pragma once

#include <base/filesystem.h>
#include <Windows.h>

fs::path get_self_path();
const char* search_api(const char* api1, const char* api2);

template <class T>
struct hook_helper
{
	static uintptr_t real;
	static void init(HMODULE module, const char* api_name)
	{
		real = (uintptr_t)GetProcAddress(module, api_name);
		base::hook::install(&real, (uintptr_t)T::fake);
	}
};
template <class T>
uintptr_t hook_helper<T>::real = 0;
