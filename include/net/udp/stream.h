#pragma once

#include <net/poller/event.h>
#include <net/poller/io_object.h>
#include <net/log/logging.h>
#include <array>

namespace bee::net { namespace udp {

	typedef std::array<char, 1512> recvbuf_t;

	template <class Poller>
	class stream_t
		: public poller::io_object_t<Poller>
		, public poller::event_t
	{
		typedef poller::io_object_t<Poller> base_type;
		typedef poller::event_t event_type;
	
	public:
		stream_t(Poller* poll)
			: base_type(poll, this)
		{
			event_type::sock = socket::retired_fd;
		}

		virtual void event_recv(recvbuf_t* rb)
		{
			delete rb;
		}

		bool event_in()
		{
			std::unique_ptr<recvbuf_t> rbuff(new recvbuf_t);
			memset(rbuff->data(), 0, sizeof(*rbuff));
			endpoint remote;
#if defined _WIN32
			int len = (int)remote.addrlen();
#else
			socklen_t len = remote.addrlen();
#endif
			int rc = recvfrom(event_type::sock, rbuff->data() + 2, (int)(rbuff->size() - 12), 0, remote.addr(), &len);
			if (rc > 0)
			{
				*(uint16_t*)rbuff->data() = (uint16_t)rc;
				*(uint32_t*)(rbuff->data()+1502) = remote.address();
				event_recv(rbuff.release());
			}
			return true;
		}

		int32_t send(const char* sbuff, int len, const endpoint &ep)
		{
			return sendto(event_type::sock, sbuff, len, 0, ep.addr(), (int)ep.addrlen());
		}

		bool event_out(){ return true;}
		void event_close(){}
		void event_timer(uint64_t id){}
	};
}}
