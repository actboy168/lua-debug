#pragma once

#if defined(_M_X64)
#    error unsupport x86_64
#endif

#include <Windows.h>

#include <string>
#include <string_view>

bool injectdll(const PROCESS_INFORMATION& pi, const std::wstring& x86dll, const std::wstring& x64dll, const std::string_view& entry = 0);
bool injectdll(DWORD pid, const std::wstring& x86dll, const std::wstring& x64dll, const std::string_view& entry = 0);
