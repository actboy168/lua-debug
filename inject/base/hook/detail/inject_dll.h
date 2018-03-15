#pragma once

#if !defined(_M_X64)
#include <base/config.h>
#include <base/filesystem.h>
#include <Windows.h>

namespace base { namespace hook { namespace detail {
	bool injectdll(HANDLE process, HANDLE thread, const fs::path& dll);
}}}
#endif
