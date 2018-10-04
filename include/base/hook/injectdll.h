#pragma once

#if !defined(_M_X64)
#include <base/config.h>
#include <base/filesystem.h>
#include <Windows.h>

namespace base { namespace hook {
	bool openprocess(DWORD pid, DWORD process_access, DWORD thread_access, PROCESS_INFORMATION& pi);
	bool injectdll(const PROCESS_INFORMATION& pi, const fs::path& x86dll, const fs::path& x64dll);
	bool injectdll(DWORD pid, const fs::path& x86dll, const fs::path& x64dll);
}}
#endif
