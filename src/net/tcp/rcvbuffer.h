#pragma once

#include <net/tcp/buffer.h>

namespace net { namespace tcp {

	class rcvbuffer
	{
	public:
		rcvbuffer()
			: rcvbuf_()
			, length_(0)
		{ }
		
		void clear()
		{
			rcvbuf_.clear();
			length_ = 0;
		}

		bool rcv(socket::fd_t s)
		{
			assert(rcvbuf_.back_pos < rcvbuf_.chunk_size());
			int rc = recv(s, (char*)&(rcvbuf_.back_chunk->values[rcvbuf_.back_pos]), (int)(rcvbuf_.chunk_size() - rcvbuf_.back_pos), 0);
			if (rc == 0)
			{
				return false;
			}
			else if (rc < 0)
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

			rcvbuf_.back_pos += rc - 1;
			rcvbuf_.do_push();
			length_ += rc;
			return true;
		}

		size_t pop(char* buf, size_t buflen)
		{
			assert(buf);
			if (buflen == 0)
			{
				return 0;
			}

			if (rcvbuf_.empty())
			{
				return 0;
			}

			size_t len = 0;
			if (rcvbuf_.begin_chunk != rcvbuf_.back_chunk)
			{
				len = rcvbuf_.chunk_size() - rcvbuf_.begin_pos;
				if (len >= buflen)
				{
					len = buflen;
				}
				else
				{
					size_t r = pop(buf, len);
					return r + pop(buf + len, buflen - len);
				}
			}
			else
			{
				len = rcvbuf_.back_pos - rcvbuf_.begin_pos;
				if (len >= buflen)
				{
					len = buflen;
				}
			}

			memcpy(buf, &(rcvbuf_.begin_chunk->values[rcvbuf_.begin_pos]), len);
			rcvbuf_.begin_pos += len - 1;
			rcvbuf_.do_pop();
			length_ -= len;
			return len;
		}

		size_t pop(size_t buflen)
		{
			if (buflen == 0)
			{
				return 0;
			}

			if (rcvbuf_.empty())
			{
				return 0;
			}

			size_t len = 0;
			if (rcvbuf_.begin_chunk != rcvbuf_.back_chunk)
			{
				len = rcvbuf_.chunk_size() - rcvbuf_.begin_pos;
				if (len >= buflen)
				{
					len = buflen;
				}
				else
				{
					size_t r = pop(len);
					return r + pop(buflen - len);
				}
			}
			else
			{
				len = rcvbuf_.back_pos - rcvbuf_.begin_pos;
				if (len >= buflen)
				{
					len = buflen;
				}
			}

			rcvbuf_.begin_pos += len - 1;
			rcvbuf_.do_pop();
			length_ -= len;
			return len;
		}

		bool empty() const
		{
			return rcvbuf_.empty();
		}

		size_t size() const
		{
			return length_;
		}

	private:
		buffer<char, 4096> rcvbuf_;
		size_t length_;
	};
}}
