#pragma once

#include <net/socket.h>
#include <assert.h>
#if defined WIN32
#	include <Mstcpip.h>
#else
#	include <fcntl.h>
#	include <netinet/tcp.h>
#	include <signal.h>
#endif

namespace net { namespace socket {

	NET_INLINE void initialize()
	{
		static bool initialized = false;
		if (!initialized) {
			initialized = true;
#if defined WIN32
			WSADATA wd;
			int rc = WSAStartup(MAKEWORD(2, 2), &wd);
			assert(rc >= 0);
#else
			struct sigaction sa;
			sa.sa_handler = SIG_IGN;
			sigaction(SIGPIPE, &sa, 0);
#endif
		}
	}

	NET_INLINE int error_no_()
	{
#if defined WIN32
		return ::WSAGetLastError();
#else
		return errno;
#endif
	}

#if defined WIN32
	NET_INLINE int wsa_error_to_errno(int errcode)
	{
		switch (errcode) {
		//  10009 - File handle is not valid.
		case WSAEBADF:
			return EBADF;
		//  10013 - Permission denied.
		case WSAEACCES:
			return EACCES;
		//  10014 - Bad address.
		case WSAEFAULT:
			return EFAULT;
		//  10022 - Invalid argument.
		case WSAEINVAL:
			return EINVAL;
		//  10024 - Too many open files.
		case WSAEMFILE:
			return EMFILE;
		//  10035 - A non-blocking socket operation could not be completed immediately.
		case WSAEWOULDBLOCK:
			return EWOULDBLOCK;
		//  10036 - Operation now in progress.
		case WSAEINPROGRESS:
			return EAGAIN;
		//  10040 - Message too long.
		case WSAEMSGSIZE:
			return EMSGSIZE;
		//  10043 - Protocol not supported.
		case WSAEPROTONOSUPPORT:
			return EPROTONOSUPPORT;
		//  10047 - Address family not supported by protocol family.
		case WSAEAFNOSUPPORT:
			return EAFNOSUPPORT;
		//  10048 - Address already in use.
		case WSAEADDRINUSE:
			return EADDRINUSE;
		//  10049 - Cannot assign requested address.
		case WSAEADDRNOTAVAIL:
			return EADDRNOTAVAIL;
		//  10050 - Network is down.
		case WSAENETDOWN:
			return ENETDOWN;
		//  10051 - Network is unreachable.
		case WSAENETUNREACH:
			return ENETUNREACH;
		//  10052 - Network dropped connection on reset.
		case WSAENETRESET:
			return ENETRESET;
		//  10053 - Software caused connection abort.
		case WSAECONNABORTED:
			return ECONNABORTED;
		//  10054 - Connection reset by peer.
		case WSAECONNRESET:
			return ECONNRESET;
		//  10055 - No buffer space available.
		case WSAENOBUFS:
			return ENOBUFS;
		//  10057 - Socket is not connected.
		case WSAENOTCONN:
			return ENOTCONN;
		//  10060 - Connection timed out.
		case WSAETIMEDOUT:
			return ETIMEDOUT;
		//  10061 - Connection refused.
		case WSAECONNREFUSED:
			return ECONNREFUSED;
		//  10065 - No route to host.
		case WSAEHOSTUNREACH:
			return EHOSTUNREACH;
		default:
			net_assert(false);
		}
		//  Not reachable
		return 0;
	}
#endif

	NET_INLINE int error_no()
	{
#if defined WIN32
		return wsa_error_to_errno(error_no_());
#else
		return error_no_();
#endif
	}

	NET_INLINE fd_t open(int domain, int type, int protocol)
	{
		return ::socket(domain, type, protocol);
	}

	NET_INLINE void close(fd_t s)
	{
		assert(s != retired_fd);
#if defined WIN32
		int rc = closesocket(s);
#else
		int rc = ::close(s);
#endif
		//net_assert_success(rc);
	}

	NET_INLINE void shutdown(fd_t s)
	{
		assert(s != retired_fd);
#if defined WIN32
		int rc = ::shutdown(s, SD_BOTH);
#else
		int rc = ::shutdown(s, SHUT_RDWR);
#endif
		net_assert_success(rc);
	}

	NET_INLINE void shutdown_read(fd_t s)
	{
		assert(s != retired_fd);
#if defined WIN32
		int rc = ::shutdown(s, SD_RECEIVE);
#else
		int rc = ::shutdown(s, SHUT_RD);
#endif
		net_assert_success(rc);
	}

	NET_INLINE void shutdown_write(fd_t s)
	{
		assert(s != retired_fd);
#if defined WIN32
		int rc = ::shutdown(s, SD_SEND);
#else
		int rc = ::shutdown(s, SHUT_WR);
#endif
		net_assert_success(rc);
	}

	NET_INLINE void nonblocking(fd_t s)
	{
#if defined WIN32
		unsigned long nonblock = 1;
		int rc = ioctlsocket(s, FIONBIO, &nonblock);
		net_assert_success(rc);
#else
		int flags = fcntl(s, F_GETFL, 0);
		if (flags == -1)
			flags = 0;
		int rc = fcntl(s, F_SETFL, flags | O_NONBLOCK);
		net_assert(rc != -1);
#endif
	}

	NET_INLINE void keepalive(fd_t s, int keepalive, int keepalive_cnt, int keepalive_idle, int keepalive_intvl)
	{
#if defined WIN32
		if (keepalive != -1)
		{
			tcp_keepalive keepaliveopts;
			keepaliveopts.onoff = keepalive;
			keepaliveopts.keepalivetime = keepalive_idle != -1 ? keepalive_idle * 1000 : 7200000;
			keepaliveopts.keepaliveinterval = keepalive_intvl != -1 ? keepalive_intvl * 1000 : 1000;
			DWORD num_bytes_returned;
			int rc = WSAIoctl(s, SIO_KEEPALIVE_VALS, &keepaliveopts, sizeof(keepaliveopts), NULL, 0, &num_bytes_returned, NULL, NULL);
			net_assert_success(rc);
		}
#else
		if (keepalive != -1)
		{
			int rc = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*) &keepalive, sizeof (int));
			net_assert(rc == 0);

			if (keepalive_cnt != -1)
			{
				int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_cnt, sizeof (int));
				net_assert_success(rc);
			}
			if (keepalive_idle != -1)
			{
				int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof (int));
				net_assert_success(rc);
			}
			if (keepalive_intvl != -1)
			{
				int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof (int));
				net_assert_success(rc);
			}
		}
#endif
	}

	NET_INLINE void udp_connect_reset(fd_t s)
	{
#if defined WIN32
		DWORD byte_retruned = 0;
		bool new_be = false;
		WSAIoctl(s, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
#endif
	}

	NET_INLINE void send_buffer(fd_t s, int bufsize)
	{
		const int rc = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*) &bufsize, sizeof bufsize);
		net_assert_success(rc);
	}

	NET_INLINE void recv_buffer(fd_t s, int bufsize)
	{
		const int rc = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*) &bufsize, sizeof bufsize);
		net_assert_success(rc);
	}

	NET_INLINE void reuse(fd_t s)
	{
		int flag = 1;
#if defined WIN32
		const int rc = setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*) &flag, sizeof flag);
#else
		const int rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &flag, sizeof flag);
#endif
		net_assert_success(rc);
	}

	NET_INLINE bool connect_error(fd_t s)
	{
		int err = 0;
#if defined WIN32
		int len = sizeof(err);
#else
		socklen_t len = sizeof(err);
#endif
		int rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &err, &len);
#if defined WIN32
		assert(rc == 0);
		if (err != 0)
			return false;
#else
		if (rc == -1)
			err = error_no_();
		if (err != 0)
			return false;
#endif
		return true;
	}

	NET_INLINE int connect(fd_t s, const endpoint& ep)
	{
		int rc = ::connect(s, ep.addr(), (int)ep.addrlen());
		if (rc == 0)
			return 0;

		const int error_code = error_no_();
#if defined WIN32
		if (error_code == WSAEINPROGRESS || error_code == WSAEWOULDBLOCK)
			return -2;
#else
		if (error_code == EINTR || error_code == EINPROGRESS)
			return -2;
#endif
		return -1;
	}

	NET_INLINE int bind(fd_t s, const endpoint& ep)
	{
		return ::bind(s, ep.addr(), (int)ep.addrlen());
	}

	NET_INLINE int listen(fd_t s, const endpoint& ep, int backlog)
	{
		if (::bind(s, ep.addr(), (int)ep.addrlen()) == -1)
		{
			return -1;
		}
		if (::listen(s, backlog) == -1)
		{
			return -2;
		}
		return 0;
	}

	NET_INLINE int accept(fd_t s, fd_t& fd, endpoint& ep)
	{
		struct sockaddr_storage ss;
		memset(&ss, 0, sizeof (ss));
#if defined WIN32
		int ss_len = sizeof (ss);
#else
		socklen_t ss_len = sizeof (ss);
#endif
		fd = ::accept(s, (struct sockaddr *) &ss, &ss_len);
		if (fd == retired_fd)
		{
#if defined WIN32
			return -1;
#else 
			const int error_code = error_no_();
			if (error_code != EAGAIN && error_code != ECONNABORTED && error_code != EPROTO && error_code != EINTR)
			{
				return -1;
			}
			else 
			{
				return -2;
			}
#endif
		}
#if defined WIN32
		assert(ss_len > 0);
#endif
		assert((size_t)ss_len >= ep.addrlen());
		memcpy(ep.addr(), &ss, ep.addrlen());
		return 0;
	}
}}
