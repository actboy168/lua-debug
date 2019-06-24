#pragma once

#if !defined(_M_X64)
#include <string>
#include <Windows.h>

bool injectdll(const PROCESS_INFORMATION& pi, const std::wstring& x86dll, const std::wstring& x64dll, const char* entry = 0);
bool injectdll(DWORD pid, const std::wstring& x86dll, const std::wstring& x64dll, const char* entry = 0);
#endif
