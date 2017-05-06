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
			return (char*)&(rcvbuf_.back());
		}

		size_t rcv_size() const
		{
			return rcvbuf_.back_size();
		}

		void rcv_push(size_t n)
		{
			rcvbuf_.do_push(n);
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

			size_t len = rcvbuf_.front_size();
			if (len == 0) { }
			else if (len >= buflen)
			{
				len = buflen;
			}
			else
			{
				size_t r = pop(buf, len);
				return r + pop(buf + len, buflen - len);
			}

			memcpy(buf, &(rcvbuf_.front()), len);
			rcvbuf_.do_pop(len);
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

			size_t len = rcvbuf_.front_size();
			if (len == 0) {}
			else if (len >= buflen)
			{
				len = buflen;
			}
			else
			{
				size_t r = pop(len);
				return r + pop(buflen - len);
			}

			rcvbuf_.do_pop(len);
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
