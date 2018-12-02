#pragma once

#include <functional>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <net/poller.h>
#include <net/poller/event.h>
#include <net/poller/io_object.h>
#include <net/log/logging.h>

#if defined _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4355)
#endif

namespace bee::net { namespace tcp {
	template <class Poller>
	class listener_t
		: public poller::io_object_t<Poller>
		, public poller::event_t
	{
		typedef poller::io_object_t<Poller> base_t;
		typedef poller::event_t event_type;

	public:
		enum stat_t {
			e_idle,
			e_listening,
		};

		listener_t(Poller* poll)
			: base_t(poll, this)
			, stat_(e_idle)
		{
			event_type::sock = socket::retired_fd;
		}

		~listener_t()
		{
			close();
		}

		int open(const endpoint& addr)
		{
			close();
			event_type::sock = socket::open(addr.addr()->sa_family, socket::protocol::tcp);
			if (event_type::sock == socket::retired_fd)
			{
				NETLOG_ERROR() << "socket("<< event_type::sock << ") socket open error, ec = " << socket::errcode();
				return -1;
			}

			socket::nonblocking(event_type::sock);
			socket::reuse(event_type::sock);
			int bufsize = 64 * 1024;
			socket::send_buffer(event_type::sock, bufsize);
			socket::recv_buffer(event_type::sock, bufsize);
			return 0;
		}

		virtual void event_accept(net::socket::fd_t fd) = 0;

		bool listen(const endpoint& addr)
		{
			assert(stat_ == e_idle);
			auto[ip, port] = addr.info();
			NETLOG_INFO() << "socket(" << event_type::sock << ") [" << ip << ":" << port << "] listening";
			int rc = socket::bind(event_type::sock, addr) 
				|| socket::listen(event_type::sock, 0x100);
			if (rc == 0)
			{
				stat_ = e_listening;
				base_t::set_fd(event_type::sock);
				base_t::set_pollin();
				return true;
			}
			else
			{
				NETLOG_ERROR() << "socket(" << event_type::sock << ") listen error, ec = " << socket::errcode();
				if (event_type::sock != socket::retired_fd)
				{
					event_close();
				}
				return false;
			}
		}

		void close()
		{
			event_close();
		}

		bool event_in()
		{
			socket::fd_t fd = socket::retired_fd;
			int rc = socket::accept(event_type::sock, fd);
			if (rc == 0)
			{
				event_accept(fd);
				return true;
			}
			else if (rc == -2)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool event_out()
		{
			return true;
		}

		void event_close()
		{
			if (event_type::sock == socket::retired_fd)
			{
				return;
			}

			base_t::rm_fd();
			base_t::cancel_timer();
			socket::close(event_type::sock);
			stat_ = e_idle;
			event_type::sock = socket::retired_fd;
		}

		void event_timer(uint64_t id)
		{ }

		bool is_listening() const
		{
			return stat_ == e_listening;
		}

	protected:
		stat_t stat_;
	};
	typedef listener_t<poller_t> listener;
}}

#if defined _MSC_VER
#	pragma warning(pop)
#endif
