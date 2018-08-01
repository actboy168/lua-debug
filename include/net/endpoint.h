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
		endpoint()
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
		}

		endpoint(const std::string& addr)
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
			size_t pos = addr.find(':');
			if (pos == addr.npos) {
				address(addr);
				port(0);
			}
			else {
				address(addr.substr(0, pos));
				port(atoi(addr.substr(pos + 1).c_str()));
			}
			addr_.sin_family = AF_INET;
		}

		endpoint(const std::string& hostname, uint16_t port_num)
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
			address(hostname);
			port(port_num);
			addr_.sin_family = AF_INET;
		}

		endpoint(uint32_t ip, uint16_t port_num)
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
			address(ip);
			port(port_num);
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
				, port()
			);
#else
			char ip_str[32] = { 0 };
			::inet_ntop(addr_.sin_family, &(addr_.sin_addr), ip_str, sizeof(ip_str));
			sprintf(result, "%s:%d", ip_str, port());
#endif
			return std::move(std::string(result));
		}

		std::string ip_to_string() const
		{
			char result[32] = { 0 };
#if defined(_WIN32)
			sprintf_s(result, sizeof(result) - 1, "%d.%d.%d.%d"
				, addr_.sin_addr.S_un.S_un_b.s_b1
				, addr_.sin_addr.S_un.S_un_b.s_b2
				, addr_.sin_addr.S_un.S_un_b.s_b3
				, addr_.sin_addr.S_un.S_un_b.s_b4
			);
#else
			::inet_ntop(addr_.sin_family, &(addr_.sin_addr), result, sizeof(result));
#endif
			return std::move(std::string(result));
		}

		void port(uint16_t port_num)
		{
			addr_.sin_port = htons(port_num);
		}

		uint16_t port() const
		{
			return ntohs(addr_.sin_port);
		}

		void address(uint32_t ip)
		{
			addr_.sin_addr.s_addr = htonl(ip);
		}

		void address(const std::string& hostname)
		{
			addr_.sin_addr.s_addr = inet_addr(hostname.c_str());
			if (addr_.sin_addr.s_addr == (uint32_t)-1)
			{
				hostent* hostptr = gethostbyname(hostname.c_str());
				if (hostptr) 
				{
					addr_.sin_addr.s_addr = (*reinterpret_cast<uint32_t*>(hostptr->h_addr_list[0]));
				}
			}
		}

		uint32_t address() const
		{
			return ntohl(addr_.sin_addr.s_addr);
		}

		const struct sockaddr* addr() const
		{
			return (const struct sockaddr*)&addr_;
		}

		struct sockaddr* addr()
		{
			return (struct sockaddr*)&addr_;
		}

		size_t addrlen() const
		{
			return sizeof(sockaddr_in);
		}

	public:
		template <class Reader>
		void parse(Reader& r)
		{
			memset(&addr_, 0, sizeof(struct sockaddr_in));
			uint32_t i32 = 0; r.pop(i32); address(i32);
			uint16_t i16 = 0; r.pop(i16); port(i16);
			addr_.sin_family = AF_INET;
		}

		template <class Writer>
		void serialize(Writer& w) const
		{
			w.push(address());
			w.push(port());
		}

	private:
		sockaddr_in addr_;
	};
}
