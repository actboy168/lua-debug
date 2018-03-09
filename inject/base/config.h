#pragma once

#define _BASE_API

#if defined(_BASE_EXPORTS)
#	define _BASE_EXPORT __declspec(dllexport)
#else
#	define _BASE_EXPORT __declspec(dllimport)
#endif
