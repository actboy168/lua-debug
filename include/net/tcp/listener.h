#pragma once

#include <functional>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <net/poller.h>
#include <net/poller/event.h>
#include <net/poller/io_object.h>

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
			event_type::sock = socket::open(addr.family() == AF_UNIX ? socket::protocol::unix : socket::protocol::tcp, addr);
			if (event_type::sock == socket::retired_fd)
			{
				return -1;
			}

			if (addr.addr()->sa_family != AF_UNIX) {
				socket::reuse(event_type::sock);
			}
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
            if (socket::bind(event_type::sock, addr) == socket::status::success
				&& socket::listen(event_type::sock, 0x100) == socket::status::success)
			{
				stat_ = e_listening;
				base_t::set_fd(event_type::sock);
				base_t::set_pollin();
				return true;
			}
			else
			{
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
            switch (socket::accept(event_type::sock, fd)) {
            case socket::status::success:
                event_accept(fd);
                return true;
            case socket::status::wait:
                return true;
            default:
            case socket::status::failed:
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
