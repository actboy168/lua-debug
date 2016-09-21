#pragma once

#if defined WIN32
#	include <net/poller/select.h>
#else
#	include <net/poller/epoll.h>
#endif

namespace net {
#if defined WIN32
	typedef poller::select_t poller_t;
#else
	typedef poller::epoll_t poller_t;
#endif
}
