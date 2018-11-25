#pragma once

#include <debugger/io/stream.h>
#include <stdint.h>

namespace net {
	struct poller_t;
}

namespace vscode { namespace io {
	class sock_server;
	class sock_client;
	class sock_session;

	class DEBUGGER_API sock_stream
		: public stream
	{
	public:
		sock_stream();
		size_t raw_peek();
		bool raw_recv(char* buf, size_t len);
		bool raw_send(const char* buf, size_t len);
		void open(sock_session* s);
		void close();
		bool is_closed() const;

	private:
		sock_session* s;
	};

	class DEBUGGER_API socket_s
		: public sock_stream
	{
	public:
		socket_s(const std::string& ip, uint16_t port);
		virtual   ~socket_s();
		void      update(int ms);
		void      close();
		uint16_t  get_port() const;

	private:
		net::poller_t* poller_;
		sock_server*   server_;
	};

	class DEBUGGER_API socket_c
		: public sock_stream
	{
	public:
		socket_c(const std::string& ip, uint16_t port);
		virtual   ~socket_c();
		void      update(int ms);
		void      close();

	private:
		net::poller_t* poller_;
		sock_client*   client_;
	};
}}
