#pragma once
#include <string>

#ifdef _WIN32
#if !defined(_M_X64)
#include <Windows.h>

bool injectdll(const PROCESS_INFORMATION& pi, const std::wstring& x86dll, const std::wstring& x64dll, const char* entry = 0);
bool injectdll(DWORD pid, const std::wstring& x86dll, const std::wstring& x64dll, const char* entry = 0);
#endif
#else
#include <unistd.h>
bool injectdll(pid_t pid, const std::string& dll, const char* entry = 0);
#endif
