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

		char* rcv_data() 
		{
			return (char*)&(rcvbuf_.back_chunk->values[rcvbuf_.back_pos]);
		}

		size_t rcv_size() const
		{
			assert(rcvbuf_.back_pos < rcvbuf_.chunk_size());
			return rcvbuf_.chunk_size() - rcvbuf_.back_pos;
		}

		void rcv_push(size_t n)
		{
			assert(n > 0);
			rcvbuf_.back_pos += n - 1;
			rcvbuf_.do_push();
			length_ += n;
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
		queue<char, 4096> rcvbuf_;
		size_t length_;
	};
}}
