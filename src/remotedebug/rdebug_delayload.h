#pragma once

#if defined(_MSC_VER)

#include <Windows.h>
#include <intrin.h>

namespace remotedebug::delayload {
	void set_luadll(HMODULE handle);
	void caller_is_luadll(void* callerAddress);
}

#endif
