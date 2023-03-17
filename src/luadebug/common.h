#pragma once

#if defined(_WIN32)
#    define RLUA_FUNC extern "C" __declspec(dllexport)
#else
#    define RLUA_FUNC extern "C" __attribute__((visibility("default")))
#endif
