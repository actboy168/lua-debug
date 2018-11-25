#pragma once

#include <net/endpoint.h>
#include <net/socket.h>
#include <net/tcp/sndbuffer.h>
#include <net/tcp/rcvbuffer.h> 
#include <net/poller.h>
#include <net/poller/event.h>
#include <net/poller/io_object.h>
#include <net/log/logging.h>

#if defined _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4355)
#endif

namespace net { namespace tcp {

	template <class Poller>
	class stream_t
		: public poller::io_object_t<Poller>
		, public poller::event_t
	{
		typedef poller::io_object_t<Poller> base_t;
		typedef poller::event_t event_type;

	public:
		stream_t(Poller* poll)
			: base_t(poll, this)
			, sndbuf_()
			, rcvbuf_()
			, wait_close_(false)
		{
			event_type::sock = socket::retired_fd;
		}

		void attach(socket::fd_t s)
		{
			NETLOG_INFO() << "socket(" << s << ") connected";
			event_type::sock = s;
			base_t::set_fd(event_type::sock);
			base_t::set_pollin();
			if (!write_empty())
			{
				base_t::set_pollout();
			}
			rcvbuf_.clear();
			event_connected();
		}

		void clear()
		{
			sndbuf_.clear();
			rcvbuf_.clear();
		}

		virtual void event_connected()
		{ }
		
		void close()
		{
			if (event_type::sock != socket::retired_fd)
			{
				event_close();
			}
		}

		size_t send(const char* buf, size_t buflen)
		{
			if (wait_close_)
			{
				return 0;
			}
			size_t r = sndbuf_.push(buf, buflen);
			if (write_empty())
			{
				base_t::reset_pollout();
			}
			else
			{
				base_t::set_pollout();
			}
			return r;
		}

		template <class T>
		size_t send(const T& t)
		{
			return send(t.data(), t.size());
		}

		template <size_t n>
		size_t send(const char (&buf)[n])
		{
			return send(buf, n-1);
		}

		template <size_t n>
		size_t send(char (&buf)[n]);

		bool write_empty() const
		{
			return sndbuf_.empty();
		}

		size_t recv(char* buf, size_t buflen)
		{
			return rcvbuf_.pop(buf, buflen);
		}

		size_t recv(size_t buflen)
		{
			return rcvbuf_.pop(buflen);
		}

		size_t recv_size() const
		{
			return rcvbuf_.size();
		}

		bool event_in()
		{
			int rc = ::recv(event_type::sock, rcvbuf_.rcv_data(), (int)rcvbuf_.rcv_size(), 0);
			if (rc == 0)
			{
				NETLOG_ERROR() << "socket(" << event_type::sock << ") recv error, ec = " << socket::error_no_();
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
					NETLOG_ERROR() << "socket(" << event_type::sock << ") recv error, ec = " << socket::error_no_();
					return false;
				}
			}

			rcvbuf_.rcv_push(rc);
			return true;
		}

		bool event_out()
		{
			if (sndbuf_.empty())
			{
				if (wait_close_) force_close();
				return true;
			}

			int rc = ::send(event_type::sock, sndbuf_.snd_data(), (int)sndbuf_.snd_size(), 0);
			if (rc < 0)
			{
				int ec = socket::error_no();
				if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR)
				{
					return true;
				}
				else
				{
					NETLOG_ERROR() << "socket(" << event_type::sock << ") send error, ec = " << socket::error_no_();
					return false;
				}
			}

			sndbuf_.snd_pop(rc);
			if (write_empty())
			{
				base_t::reset_pollout();
				if (wait_close_) force_close();
			}
			return true;
		}

		void event_close()
		{
			wait_close_ = true;
			if (sndbuf_.empty())
			{
				force_close();
			}
		}

		void force_close()
		{
			NETLOG_INFO() << "socket(" << event_type::sock << ") close";
			base_t::rm_fd();
			base_t::cancel_timer();
			socket::close(event_type::sock);
			event_type::sock = socket::retired_fd;
			clear();
		}

		void event_timer(uint64_t id)
		{
		}

	protected:
		sndbuffer sndbuf_;
		rcvbuffer rcvbuf_;
		bool      wait_close_;
	};
	typedef stream_t<poller_t> stream;
}}

#if defined _MSC_VER
#	pragma warning(pop)
#endif
