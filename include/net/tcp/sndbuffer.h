#pragma once

#include <net/tcp/buffer.h>

namespace net { namespace tcp {

	class sndbuffer
	{
	public:
		sndbuffer()
			: sndbuf_()
			, length_(0)
		{ }

		void clear()
		{
			sndbuf_.clear();
			length_ = 0;
		}

		const char* snd_data() const
		{
			return (const char*)&(sndbuf_.front());
		}

		size_t snd_size() const
		{
			return sndbuf_.front_size();
		}

		void snd_pop(size_t n)
		{
			sndbuf_.do_pop(n);
			length_ -= n;
		}

		size_t push(const char* buf, size_t buflen)
		{
			if (buflen == 0)
			{
				return 0;
			}

			size_t len = sndbuf_.back_size();
			if (len < buflen)
			{
				size_t r = push(buf, len);
				return r + push(buf + len, buflen - len);
			}

			memcpy(&(sndbuf_.back()), buf, buflen);
			sndbuf_.do_push(buflen);
			length_ += buflen;
			return buflen;
		}

		bool empty() const
		{
			return sndbuf_.empty();
		}

		size_t size() const
		{
			return length_;
		}

	private:
		buffer<char, 4096> sndbuf_;
		size_t length_;
	};
}}
