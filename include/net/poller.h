#pragma once

#if defined WIN32
#	include <net/poller/select.h>
#else
#	include <net/poller/epoll.h>
#endif

namespace net {
#if defined WIN32
	struct poller_t : public poller::select_t { };
#else						  
	struct poller_t : public poller::epoll_t { };
#endif
}
