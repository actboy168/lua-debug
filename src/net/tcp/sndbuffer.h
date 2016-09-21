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

		bool snd(socket::fd_t s)
		{
			if (sndbuf_.empty())
			{
				return true;
			}

			char* buf = (char*)&(sndbuf_.begin_chunk->values[sndbuf_.begin_pos]);
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

			int rc = send(s, buf, (int)len, 0);
			if (rc < 0)
			{
				int ec = socket::error_no();
				if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR)
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			sndbuf_.begin_pos += rc - 1;
			sndbuf_.do_pop();
			length_ -= rc;
			return true;
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
		buffer<char, 4096> sndbuf_;
		size_t length_;
	};
}}
