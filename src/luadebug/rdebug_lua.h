#pragma once

#if defined(_WIN32)
#    define LUADEBUG_FUNC extern "C" __declspec(dllexport)
#else
#    define LUADEBUG_FUNC extern "C" __attribute__((visibility("default")))
#endif

#include "luadbg/inc/luadbg.hpp"
#include "rdebug_luacompat.h"
