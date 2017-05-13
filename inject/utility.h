#pragma once

#include <base/filesystem.h>
#include <Windows.h>

fs::path get_self_path();
const char* search_api(const char* api1, const char* api2);

template <class T>
struct hook_helper
{
	static uintptr_t real;
	static void init(const wchar_t* module_path, const char* api_name)
	{
		fs::path dllname = fs::path(module_path).filename();
		HMODULE module_handle = GetModuleHandleW(dllname.c_str());
		if (module_handle) {
			base::hook::iat(GetModuleHandleW(NULL), dllname.string().c_str(), api_name, (uintptr_t)T::fake);
		}
		real = base::hook::eat(module_path, api_name, (uintptr_t)T::fake);
	}

	static void init(const char* module_name, const char* api_name)
	{
		HMODULE module_handle = GetModuleHandleA(module_name);
		if (module_handle) {
			base::hook::iat(GetModuleHandleW(NULL), module_name, api_name, (uintptr_t)T::fake);
			real = base::hook::eat(module_handle, api_name, (uintptr_t)T::fake);
		}
		else {
			real = base::hook::eat(LoadLibraryA(module_name), api_name, (uintptr_t)T::fake);
		}
	}
};
template <class T>
uintptr_t hook_helper<T>::real = 0;
