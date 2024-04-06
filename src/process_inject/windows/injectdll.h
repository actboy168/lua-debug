#pragma once

#if defined(_M_X64)
#    error unsupport x86_64
#endif

#include <Windows.h>

#include <string>
#include <bee/utility/zstring_view.h>

bool injectdll(HANDLE process_handle, HANDLE thread_handle, const std::wstring& x86dll, const std::wstring& x64dll, const bee::zstring_view& entry = 0);
bool injectdll(DWORD pid, const std::wstring& x86dll, const std::wstring& x64dll, const bee::zstring_view& entry = 0);
