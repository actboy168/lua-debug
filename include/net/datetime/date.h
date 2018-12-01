#pragma once

#include <time.h> 
#include <stdio.h>
#include <net/datetime/now.h>

namespace bee::net { namespace datetime {
	struct date
		: public tm
	{
	public:
		typedef date class_type;

		date()
		{
			*(tm*)this = now();
		}
	
		date(uint64_t utc)
		{
			from_utc(utc);
		}
	
		date(const char* buf)
		{
			from_string(buf);
		}
	
		char const* to_string()
		{
			static char str[64] = { 0 };
#if defined _MSC_VER
#endif
			sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", tm_year + 1900, tm_mon + 1, tm_mday, tm_hour, tm_min, tm_sec);
	
			return str;
		}
	
		uint64_t to_utc() const
		{
			return (uint64_t)mktime((tm*)this);
		}
	
		void from_string(const char* buf)
		{
			this->tm_year = conv_num(buf + 0, buf + 4) - 1900;
			this->tm_mon = conv_num(buf + 5, buf + 7) - 1;
			this->tm_mday = conv_num(buf + 8, buf + 10);
			this->tm_hour = conv_num(buf + 11, buf + 13);
			this->tm_min = conv_num(buf + 14, buf + 16);
			this->tm_sec = conv_num(buf + 17, buf + 19);
			this->tm_yday = 0;
			this->tm_wday = 0;
			this->tm_isdst = 0;
	
			// fix tm_yday & tm_wday
			from_utc(to_utc());
		}

		void from_utc(uint64_t utc)
		{
#if defined _MSC_VER
			::localtime_s(this, (time_t*)&utc);
#else
			::localtime_r((time_t*)&utc, this);
#endif
		}
	
		static int conv_num(const char* first, const char* last)
		{
			int result = 0;
			if (*first >= '0' && *first <= '9') {
				do {
					result *= 10;
					result += *first++ - '0';
				} while (first < last && *first >= '0' && *first <= '9');
			}
			return result;
		}
	
		bool operator == (const class_type& rht) const
		{
			if (this->tm_year != rht.tm_year) return false;
			if (this->tm_mon != rht.tm_mon) return false;
			if (this->tm_mday != rht.tm_mday) return false;
			if (this->tm_hour != rht.tm_hour) return false;
			if (this->tm_min != rht.tm_min) return false;
			if (this->tm_sec != rht.tm_sec) return false;
			return true;
		}
	
		bool operator < (const class_type& rht) const
		{
			if (this->tm_year != rht.tm_year) return this->tm_year < rht.tm_year;
			if (this->tm_mon != rht.tm_mon) return this->tm_mon < rht.tm_mon;
			if (this->tm_mday != rht.tm_mday) return this->tm_mday < rht.tm_mday;
			if (this->tm_hour != rht.tm_hour) return this->tm_hour < rht.tm_hour;
			if (this->tm_min != rht.tm_min) return this->tm_min < rht.tm_min;
			if (this->tm_sec != rht.tm_sec) return this->tm_sec < rht.tm_sec;
			return false;
		}
	
		bool operator != (const class_type& rht) const
		{
			return !(*this == rht);
		}
	
		bool operator <= (const class_type& rht) const
		{
			return ((*this < rht) || (*this == rht));
		}
	
		bool operator >= (const class_type& rht) const
		{
			return !(*this < rht);
		}
	
		bool operator > (const class_type& rht) const
		{
			return (!(*this < rht) && !(*this == rht));
		}
	};
}}
