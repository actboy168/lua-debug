#pragma once

#include <stdint.h>
#include <string>

#if defined _WIN32
#	include <winsock2.h>
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <memory.h>
#endif

namespace net {
	class endpoint
	{
	public:
		endpoint(const std::string& hostname, uint16_t port_num)
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
			addr_.sin_addr.s_addr = inet_addr(hostname.c_str());
			if (addr_.sin_addr.s_addr == (uint32_t)-1)
			{
				hostent* hostptr = gethostbyname(hostname.c_str());
				if (hostptr)
				{
					addr_.sin_addr.s_addr = (*reinterpret_cast<uint32_t*>(hostptr->h_addr_list[0]));
				}
			}
			addr_.sin_port = htons(port_num);
			addr_.sin_family = AF_INET;
		}

		std::string to_string() const
		{
			char result[32] = { 0 };
#if defined(_WIN32)
			sprintf_s(result, sizeof(result) - 1, "%d.%d.%d.%d:%d"
				, addr_.sin_addr.S_un.S_un_b.s_b1
				, addr_.sin_addr.S_un.S_un_b.s_b2
				, addr_.sin_addr.S_un.S_un_b.s_b3
				, addr_.sin_addr.S_un.S_un_b.s_b4
				, ntohs(addr_.sin_port)
			);
#else
			char ip_str[32] = { 0 };
			::inet_ntop(addr_.sin_family, &(addr_.sin_addr), ip_str, sizeof(ip_str));
			sprintf(result, "%s:%d", ip_str, ntohs(addr_.sin_port));
#endif
			return std::move(std::string(result));
		}

		const struct sockaddr* addr() const
		{
			return (const struct sockaddr*)&addr_;
		}

		size_t addrlen() const
		{
			return sizeof(sockaddr_in);
		}

	private:
		sockaddr_in addr_;
	};
}
