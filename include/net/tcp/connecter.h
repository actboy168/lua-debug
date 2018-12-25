#pragma once

#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <net/tcp/sndbuffer.h>
#include <net/tcp/rcvbuffer.h>
#include <net/tcp/stream.h>
#include <net/poller.h>
#include <net/poller/event.h>
#include <net/poller/io_object.h>
#include <functional>
#include <optional>

#if defined _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4355)
#endif

namespace bee::net { namespace tcp {

	template <class Poller>
	class connecter_t
		: public poller::io_object_t<Poller>
		, public poller::event_t
	{
		typedef poller::io_object_t<Poller> base_t;
		typedef poller::event_t event_type;

		enum {
			e_reconnect_tid = 0x01,
			e_connect_tid = 0x02,
		};

	public:
		connecter_t(Poller* poll)
			: base_t(poll, this)
			, connected_()
			, addr_()
		{
			event_type::sock = socket::retired_fd;
		}

		void connect(const endpoint& target, std::function<void(socket::fd_t)> connected)
		{
			assert(!!connected);
			connected_ = connected;
			addr_ = target;
			start_connecting();
		}

		void reconnect()
		{
			base_t::add_timer(2 * 1000, e_reconnect_tid);
		}

		void close()
		{
			event_close();
		}

		bool event_in()
		{
			return event_out();
		}

		bool event_out()
		{
			base_t::cancel_timer();
			int err = socket::errcode(event_type::sock);
			base_t::rm_fd();
			if (err != 0)
			{
				event_close();
				reconnect();
				return true;
			}

			socket::fd_t s = event_type::sock;
			event_type::sock = socket::retired_fd;
			connected_(s);
			return true;
		}

		void event_close()
		{
			base_t::rm_fd();
			base_t::cancel_timer();
			socket::close(event_type::sock);
			event_type::sock = socket::retired_fd;
		}

		void event_timer(uint64_t id)
		{
			if (id == e_reconnect_tid)
			{
				if (event_type::sock != socket::retired_fd)
				{
					event_close();
				}
				start_connecting();
			}
			else if (id == e_connect_tid)
			{
				event_close();
				start_connecting();
			}
		}

	protected:
        void start_connecting()
        {
            switch (start_connect()) {
			case socket::status::success:
                base_t::set_fd(event_type::sock);
                event_out();
                break;
            case socket::status::wait:
                base_t::set_fd(event_type::sock);
                base_t::set_pollout();
                base_t::add_timer(12 * 1000, e_connect_tid);
                break;
            default:
            case socket::status::failed:
                if (event_type::sock != socket::retired_fd)
                {
                    event_close();
                }
                reconnect();
                break;
            }
		}
		
		socket::status start_connect()
		{
			assert(event_type::sock == socket::retired_fd);
			event_type::sock = socket::open(addr_->family() == AF_UNIX ? socket::protocol::unix : socket::protocol::tcp, *addr_);
			auto [ip, port] = addr_->info();

			if (event_type::sock == socket::retired_fd)
			{
				return socket::status::failed;
			}

			int bufsize = 64 * 1024;
			socket::send_buffer(event_type::sock, bufsize);
			socket::recv_buffer(event_type::sock, bufsize);

			return socket::connect(event_type::sock, *addr_);
		}

	protected:
		std::function<void(socket::fd_t)> connected_;
		std::optional<endpoint> addr_;
	};

	template <class Stream>
	class basic_connecter_t
		: public Stream
	{
		typedef Stream base_type;

	public:
		basic_connecter_t(typename Stream::poller_type* p)
			: base_type(p)
			, impl_(p)
		{ }

		void connect(const endpoint& ep)
		{
			base_type::clear();
			impl_.connect(ep, std::bind(&base_type::attach, this, std::placeholders::_1));
		}
		
		void reconnect()
		{
			base_type::clear();
			impl_.reconnect();
		}

	protected:
		connecter_t<typename Stream::poller_type> impl_;
	};

	typedef basic_connecter_t<stream> connecter;
}}

#if defined _MSC_VER
#	pragma warning(pop)
#endif
