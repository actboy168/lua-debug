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
			return (const char*)&(sndbuf_.begin_chunk->values[sndbuf_.begin_pos]);
		}

		size_t snd_size() const
		{
			size_t len = 0;
			if (sndbuf_.begin_chunk == sndbuf_.back_chunk)
			{
				len = sndbuf_.back_pos - sndbuf_.begin_pos;
			}
			else
			{
				len = sndbuf_.chunk_size() - sndbuf_.begin_pos;
			}
			assert(len > 0);
			return len;
		}

		void snd_pop(size_t n)
		{
			assert(n > 0);
			sndbuf_.begin_pos += n - 1;
			sndbuf_.do_pop();
			length_ -= n;
		}

		size_t push(const char* buf, size_t buflen)
		{
			if (buflen == 0)
			{
				return 0;
			}

			size_t len = sndbuf_.chunk_size() - sndbuf_.back_pos;
			if (len < buflen)
			{
				size_t r = push(buf, len);
				return r + push(buf + len, buflen - len);
			}

			memcpy(&(sndbuf_.back_chunk->values[sndbuf_.back_pos]), buf, buflen);
			sndbuf_.back_pos += buflen - 1;
			sndbuf_.do_push();
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
		queue<char, 4096> sndbuf_;
		size_t length_;
	};
}}
