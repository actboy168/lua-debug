#pragma once

#define NETLOG_BACKEND NETLOG_EMPTY_BACKEND

#include <iostream>
#include <thread> 	
#include <net/tcp/listener.h> 
#include <net/tcp/stream.h>	
#include <rapidjson/schema.h>
#include "dbg_protocol.h"

namespace vscode
{
	class server;

	class session
		: public net::tcp::stream
	{
	public:
		typedef net::tcp::stream base_type;

	public:
		session(server* server, net::poller_t* poll);
		bool      output(const wprotocol& wp);
		rprotocol input();
		bool      input_empty() const;
		bool      open_schema(const std::string& schema_file);

	private:
		void event_close();
		bool event_in();
		bool event_message(const std::string& buf, size_t start, size_t count);
		bool unpack();

	private:
		server* server_;
		std::string stream_buf_;
		size_t      stream_stat_;
		size_t      stream_len_;
		net::tcp::buffer<rprotocol, 8> input_queue_;
		std::unique_ptr<rapidjson::SchemaDocument> schema_;
	};

	class server
		: public net::tcp::listener
	{
	public:
		friend class session;
		typedef net::tcp::listener base_type;

	public:
		server(net::poller_t* poll);
		~server();
		void listen(const net::endpoint& ep);

		bool      output(const wprotocol& wp);
		rprotocol input();
		bool      input_empty()	const;
		void      set_schema(const char* file);
		void      close_session();

	private:
		void event_accept(net::socket::fd_t fd, const net::endpoint& ep);
		void event_close();

	private:
		std::unique_ptr<session> session_;
		std::string              schema_file_;
	};
}
