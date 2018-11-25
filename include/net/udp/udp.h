#pragma once

#include <string>
#include <net/udp/stream.h>
#include <net/poller.h>

namespace net { namespace udp {

class udp_con
	: public stream_t<poller_t>
{
	typedef stream_t<poller_t> base_type;

public:
	udp_con(poller_t *pt):
		base_type(pt)
	{ }

	bool open(bool nonblock = true)
	{
		base_type::sock = socket::open(AF_INET, IPPROTO_UDP);
		if (base_type::sock == socket::retired_fd)
		{
			return false;
		}

		socket::udp_connect_reset(base_type::sock);
		if (nonblock)
		{
			socket::nonblocking(base_type::sock);
		}

		int bufsize = 1024 * 1024;
		socket::send_buffer(base_type::sock, bufsize);
		socket::recv_buffer(base_type::sock, bufsize);

		base_type::set_fd(base_type::sock);
		base_type::set_pollin();
		return true;
	}

	bool listen(const endpoint& ep)
	{
		return (socket::bind(base_type::sock, ep) != -1);
	}

	int32_t send(char* sbuff, uint32_t len, endpoint &ep)
	{
		return base_type::send(sbuff, len, ep);
	}
};
}}
