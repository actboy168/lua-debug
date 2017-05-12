#pragma once

#include <base/filesystem.h>

fs::path get_self_path();

template <class T>
struct eat_helper
{
	static uintptr_t real;
	static void init(const wchar_t* module_name, const char* api_name)
	{
		real = base::hook::eat(module_name, api_name, (uintptr_t)T::fake);
	}
};
template <class T>
uintptr_t eat_helper<T>::real = 0;
