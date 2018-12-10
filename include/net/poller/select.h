#pragma once

#include <WinSock2.h>
#include <bee/net/socket.h>
#include <net/poller/event.h>
#include <net/poller/timer_queue.h>
#include <algorithm>
#include <vector>

namespace bee::net { namespace poller {

	class select_t : public timer_queue {
	public:
		struct ud_t {
			socket::fd_t fd;
			bool read;
			bool write;
			event_t* e;
		};

		typedef std::vector<ud_t> fd_set_t;

		select_t()
			: timer_queue()
			, fds_()
		{ }

		void add_fd(socket::fd_t s, event_t* e, ud_t* ud) {
			ud->fd = s;
			ud->read = false;
			ud->write = false;
			ud->e = e;
			fds_.push_back(*ud);
		}
		
		void rm_fd(event_t* e, ud_t* /*ud*/) {
			fds_.erase(std::remove_if(fds_.begin(), fds_.end(), [&](const ud_t& entry)->bool { return entry.e == e; }), fds_.end());
		}

		void set_pollin(event_t* e, ud_t* ud) {
            for (auto& fd : fds_) {
				if (fd.e == e) {
					ud->read = true;
                    fd.read = true;
					return;
				}
			}
			assert(false);
		}
		void reset_pollin(event_t* e, ud_t* ud) {
			for (auto& fd : fds_) {
				if (fd.e == e) {
					ud->read = false;
                    fd.read = false;
					return;
				}
			}
			assert(false);
		}

		void set_pollout(event_t* e, ud_t* ud) {
			if (e->sock == socket::retired_fd) return;
            for (auto& fd : fds_) {
				if (fd.e == e) {
					ud->write = true;
                    fd.write = true;
					return;
				}
			}
			assert(false);
		}
		void reset_pollout(event_t* e, ud_t* ud) {
			if (e->sock == socket::retired_fd) return;
            for (auto& fd : fds_) {
				if (fd.e == e) {
					ud->write = false;
                    fd.write = false;
					return;
				}
			}
			assert(false);
		}

		bool wait(int timeout) {
			int64_t next = timer_queue::execute_timers();
			timeout = (timeout > next) ? (int)next : timeout;
			struct timeval ti;
			ti.tv_sec = timeout / 1000;
			ti.tv_usec = (timeout % 1000) * 1000;

            size_t select_n = 0;
			for (; select_n < fds_.size();) {
				fd_set rd, wt, except;
				FD_ZERO(&rd);
				FD_ZERO(&wt);
                FD_ZERO(&except);

				size_t n = 0;
				for (; n < FD_SETSIZE; ++n) {
					size_t idx = select_n + n;
					if (idx >= fds_.size()) 
						break;
					if (fds_[idx].read) {
						FD_SET(fds_[idx].fd, &rd);
					}
					if (fds_[idx].write) {
                        FD_SET(fds_[idx].fd, &wt);
                        FD_SET(fds_[idx].fd, &except);
					}
				}
				
				int ok = select(0, &rd, &wt, &except, &ti);
                if (ok <= 0) {
                    continue;
                }
                for (size_t i = 0; i < n; ++i) {
                    size_t idx = select_n + i;
                    socket::fd_t fd = fds_[idx].fd;
                    event_t* e = fds_[idx].e;
                    if (FD_ISSET(fd, &wt) || FD_ISSET(fd, &except)) {
                        if (e->sock != socket::retired_fd
                            && !e->event_out()
                            && e->sock != socket::retired_fd)
                        {
                            e->event_close();
                            continue;
                        }
                    }
                    if (FD_ISSET(fd, &rd)) {
                        if (e->sock != socket::retired_fd
                            && !e->event_in()
                            && e->sock != socket::retired_fd)
                        {
                            e->event_close();
                            continue;
                        }
                    }
                }
                select_n += n;
			}
            return true;
		}
	private:
		fd_set_t fds_;
	};
}}
