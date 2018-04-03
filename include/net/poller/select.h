#pragma once

#include <net/socket.h>
#include <net/poller/event.h>
#include <net/poller/timer_queue.h>
#include <algorithm>
#include <vector>

namespace net { namespace poller {

	class select_t
		: public timer_queue
	{
	public:
		struct ud_t
		{
			socket::fd_t fd;
			bool read;
			bool write;
			event_t* e;
		};

		typedef std::vector<ud_t> fd_set_t;

		select_t()
			: timer_queue()
			, select_n_(0)
			, fds_()
		{ }

		void add_fd(socket::fd_t s, event_t* e, ud_t* ud)
		{
			ud->fd = s;
			ud->read = false;
			ud->write = false;
			ud->e = e;
			fds_.push_back(*ud);
		}
		
		void rm_fd(event_t* e, ud_t* /*ud*/)
		{
			fds_.erase(std::remove_if(fds_.begin(), fds_.end(), [&](const ud_t& entry)->bool { return entry.e == e; }), fds_.end());
		}

		void set_pollin(event_t* e, ud_t* ud)
		{
			for (auto it = fds_.begin();  it != fds_.end(); ++it)
			{
				if (it->e == e)
				{
					ud->read = true;
					it->read = true;
					return;
				}
			}
			assert(false);
		}
		void reset_pollin(event_t* e, ud_t* ud)
		{
			for (auto it = fds_.begin();  it != fds_.end(); ++it)
			{
				if (it->e == e)
				{
					ud->read = false;
					it->read = false;
					return;
				}
			}
			assert(false);
		}

		void set_pollout(event_t* e, ud_t* ud)
		{
			if (e->sock == socket::retired_fd) return;
			for (auto it = fds_.begin();  it != fds_.end(); ++it)
			{
				if (it->e == e)
				{
					ud->write = true;
					it->write = true;
					return;
				}
			}
			assert(false);
		}
		void reset_pollout(event_t* e, ud_t* ud)
		{
			if (e->sock == socket::retired_fd) return;
			for (auto it = fds_.begin();  it != fds_.end(); ++it)
			{
				if (it->e == e)
				{
					ud->write = false;
					it->write = false;
					return;
				}
			}
			assert(false);
		}

		int wait(size_t maxn, int timeout)
		{
			int64_t next = timer_queue::execute_timers();
			timeout = (timeout > next) ? (int)next : timeout;

			struct timeval ti;
			ti.tv_sec = timeout / 1000;
			ti.tv_usec = (timeout % 1000) * 1000;

			if (maxn > FD_SETSIZE) 
			{
				maxn = FD_SETSIZE;
			}

			for (;;) 
			{
				fd_set rd, wt;
				FD_ZERO(&rd);
				FD_ZERO(&wt);

				size_t i = 0;
				for (; i < maxn; ++i)
				{
					size_t idx = select_n_ + i;
					if (idx >= fds_.size()) 
						break;
					if (fds_[idx].read)
					{
						FD_SET(fds_[idx].fd, &rd);
					}
					if (fds_[idx].write)
					{
						FD_SET(fds_[idx].fd, &wt);
					}
				}
				
				int ret = select((int)(i+1), &rd, &wt, NULL, &ti);
				if (ret <= 0)
				{
					ti.tv_sec = 0;
					ti.tv_usec = 0;
					select_n_ += maxn;
					if (select_n_ >= fds_.size())
					{
						select_n_ = 0;
						return ret;
					}
				}
				else
				{
					size_t t = 0;
					size_t from = select_n_;
					select_n_ += maxn;
					if (select_n_ >= fds_.size())
					{
						select_n_ = 0;
					}
					for (size_t i = 0; i < maxn; i++)
					{
						size_t idx = from+i;
						if (idx >= fds_.size()) 
							break;
						socket::fd_t fd = fds_[idx].fd;
						event_t* e = fds_[idx].e;
						bool read_flag = !!FD_ISSET(fd, &rd);
						bool write_flag = !!FD_ISSET(fd, &wt);
						if (write_flag)
						{
							++t;
							if (e->sock != socket::retired_fd)
							{
								if (!e->event_out() && e->sock != socket::retired_fd)
								{
									e->event_close();
									continue;
								}
							}
						}
						if (read_flag)
						{
							++t;
							if (e->sock != socket::retired_fd)
							{
								if (!e->event_in() && e->sock != socket::retired_fd)
								{
									e->event_close();
									continue;
								}
							}
						}
						if (t == ret)
							break;
					}
					return (int)t;
				}
			}
		}
	private:
		size_t   select_n_;
		fd_set_t fds_;
	};
}}
