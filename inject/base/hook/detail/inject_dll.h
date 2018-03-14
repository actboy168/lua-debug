#pragma once

#include <base/config.h>
#include <Windows.h>

#if !defined(_M_X64)
namespace base { namespace hook { namespace detail {
	bool inject_dll(HANDLE process_handle, HANDLE thread_handle, const wchar_t* dll_name);
}}}
#endif
