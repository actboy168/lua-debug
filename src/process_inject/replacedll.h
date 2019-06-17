#pragma once

#include <Windows.h>

namespace base { namespace hook {
	bool replacedll(const PROCESS_INFORMATION& pi, const char* oldDll, const char* newDll);
}}
