#pragma once

#include <net/socket.h>
#include <net/poller/event.h>
#include <net/poller/timer_queue.h>
#include <sys/epoll.h>

namespace bee::net { namespace poller {

	class epoll_t
		: public timer_queue
	{
	public:
		struct ud_t
		{
			bool read;
			bool write;
		};

		epoll_t()
			: timer_queue()
			, epfd_(epoll_create(1024))
		{
			assert(epfd_ != -1);
		}

		~epoll_t()
		{
			close(epfd_);
		}

		void add_fd(socket::fd_t s, event_t* e, ud_t* ud)
		{
			ud->read = false;
			ud->write = false;
			
			struct epoll_event ev;
			ev.events = 0;
			ev.data.ptr = e;
			int rc = epoll_ctl(epfd_, EPOLL_CTL_ADD, s, &ev);
			assert(rc != -1); (unsigned)rc;
		}
		
		void rm_fd(event_t* e, ud_t* ud)
		{
			epoll_ctl(epfd_, EPOLL_CTL_DEL, e->sock, NULL);
		}

		void set_pollin(event_t* e, ud_t* ud)
		{
			ud->read = true;
			update(e, ud);
		}

		void reset_pollin(event_t* e, ud_t* ud)
		{
			ud->read = false;
			update(e, ud);
		}

		void set_pollout(event_t* e, ud_t* ud)
		{
			if (e->sock == socket::retired_fd) return;
			ud->write = true;
			update(e, ud);
		}

		void reset_pollout(event_t* e, ud_t* ud)
		{
			if (e->sock == socket::retired_fd) return;
			ud->write = false;
			update(e, ud);
		}

		void update(event_t* e, ud_t* ud)
		{
			struct epoll_event ev;
			ev.events = 0;
			if (ud->read) ev.events |= EPOLLIN;
			if (ud->write) ev.events |= EPOLLOUT;
			ev.data.ptr = e;
			int rc = epoll_ctl(epfd_, EPOLL_CTL_MOD, e->sock, &ev);
			assert(rc != -1); (unsigned)rc;
		}

		int wait(size_t maxn, int timeout)
		{
			int64_t next = timer_queue::execute_timers();
			timeout = (timeout > next) ? next : timeout;

			struct epoll_event ev[maxn];
			int n = epoll_wait(epfd_, ev, maxn, timeout);
			for (int i = 0; i < n; ++i)
			{
				event_t* e = (event_t*)ev[i].data.ptr;
				unsigned flag = ev[i].events;
				if ((flag & EPOLLOUT) != 0)
				{
					if (e->sock != socket::retired_fd)
					{
						if (!e->event_out() && e->sock != socket::retired_fd)
						{
							e->event_close();
							continue;
						}
					}
				}
				if ((flag & EPOLLIN) != 0)
				{
					if (e->sock != socket::retired_fd)
					{
						if (!e->event_in() && e->sock != socket::retired_fd)
						{
							e->event_close();
							continue;
						}
					}
				}
			}
			return n;
		}

	private:
		int epfd_;
	};
}}
