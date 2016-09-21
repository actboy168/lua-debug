#pragma once

#include <net/endpoint.h>
#include <net/socket.h>
#include <net/tcp/sndbuffer.h>
#include <net/tcp/rcvbuffer.h>
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
			, remote_()
			, sndbuf_()
			, rcvbuf_()
		{
			event_type::sock = socket::retired_fd;
		}

		void attach(socket::fd_t s, const endpoint& ep)
		{
			NETLOG_INFO() << "socket(" << s << ") " << ep.to_string() << " connected";
			event_type::sock = s;
			remote_ = ep;
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
			bool suc = rcvbuf_.rcv(event_type::sock);
			if (!suc)
			{
				NETLOG_ERROR() << "socket(" << event_type::sock << ") recv error, ec = " << socket::error_no_();
			}
			return suc;
		}

		bool event_out()
		{
			if (!sndbuf_.snd(event_type::sock))
			{
				NETLOG_ERROR() << "socket(" << event_type::sock << ") send error, ec = " << socket::error_no_();
				return false;
			}

			if (write_empty())
			{
				base_t::reset_pollout();
			}
			return true;
		}

		void event_close()
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

		const endpoint& remote_endpoint() const
		{
			return remote_;
		}

	protected:
		endpoint  remote_;
		sndbuffer sndbuf_;
		rcvbuffer rcvbuf_;
	};
	typedef stream_t<poller_t> stream;
}}

#if defined _MSC_VER
#	pragma warning(pop)
#endif
