#pragma once

#if defined _WIN32			   
#	include <net/winapi.h>	
#	include <intrin.h>
#	pragma intrinsic(__rdtsc)
#else
#	include <sys/time.h>
#endif

#include <stdint.h>

namespace net { namespace datetime {

	class clock_t
	{
		static const uint64_t clock_precision = 1000000;
	public:
		clock_t()
			: last_tsc(0)
			, last_time(0)
#if defined _WIN32
			, get_tick_count64(init_get_tick_count64())
			, rollover_time(0)
			, lastnow_time(0)
#endif
		{ }

		uint64_t now_ms()
		{
			uint64_t tsc = rdtsc();
			if (!tsc)
			{
				return now_ms_sys();
			}
		
			if (tsc - last_tsc <= (clock_precision / 2) && tsc >= last_tsc)
			{
				return last_time;
			}
		
			last_tsc = tsc;
			last_time = now_ms_sys();
			return last_time;
		}
		
		uint64_t now_ms_sys()
		{
#if defined _WIN32
			return get_tick_count64 ? get_tick_count64() : custom_get_tick_count64();
#else
			struct timespec tv;
			int rc = clock_gettime(CLOCK_MONOTONIC, &tv);
			assert(rc == 0); (unsigned)rc;
			return (tv.tv_sec * (uint64_t)1000 + tv.tv_nsec / 1000000);
#endif
		}

		uint64_t rdtsc()
		{
#if defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
			return __rdtsc();
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
			uint32_t low, high;
			__asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
			return (uint64_t) high << 32 | low;
#else
			return 0;
#endif
		}
		
#if defined _WIN32
		private:
			typedef uint64_t (WINAPI *get_tick_count64_t)();
			get_tick_count64_t get_tick_count64;
			uint32_t           rollover_time;
			uint32_t           lastnow_time;
			
			get_tick_count64_t init_get_tick_count64()
			{
				rollover_time = 0;
				lastnow_time = 0;

				get_tick_count64_t func = 0;
				winapi::HMODULE module = winapi::LoadLibraryW(L"kernel32.dll");
				if (module != 0)
				{
					func = reinterpret_cast<get_tick_count64_t>(winapi::GetProcAddress(module, "GetTickCount64"));
				}
				return func;
			}

			uint64_t custom_get_tick_count64()
			{
				uint32_t now = winapi::GetTickCount();
				if ((now < lastnow_time) && ((lastnow_time - now) > (1 << 30)))
				{
					++rollover_time;
				}
				lastnow_time = now;
				return (((uint64_t)rollover_time) << 32) | now;
			}
#endif

		private:
			uint64_t last_tsc;
			uint64_t last_time;
	};
}}
