#pragma once

#include <bee/net/socket.h>

namespace bee::net{ namespace poller {

	struct event_t
	{
		socket::fd_t sock;
		virtual bool event_in() = 0;
		virtual bool event_out() = 0;
		virtual void event_close() = 0;
		virtual void event_timer(uint64_t id) = 0;
	};
}}
