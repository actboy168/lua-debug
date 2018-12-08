#pragma once

#include <debugger/io/stream.h>
#include <stdint.h>

#define NETLOG_BACKEND NETLOG_EMPTY_BACKEND	
#include <iostream>
#include <memory>
#include <string>	
#include <thread> 	
#include <net/poller.h>
#include <net/tcp/connecter.h>
#include <net/tcp/listener.h> 
#include <net/tcp/stream.h>	
#include <bee/utility/format.h>
#include <bee/net/socket.h>
#include <debugger/io/stream.h>

namespace bee::net {
	struct poller_t;
	struct endpoint;
}

namespace vscode { namespace io {

    class session
        : public stream
        , public bee::net::tcp::stream
    {
    public:
        session(bee::net::poller_t* poller);
        virtual   ~session();
        bool      is_closed() const;
        void      close();
        bool      event_in();
    
        size_t raw_peek();
        bool raw_recv(char* buf, size_t len);
        bool raw_send(const char* buf, size_t len);
    };

	class server
        : public bee::net::tcp::listener
        , public base
	{
	public:
        server(const bee::net::endpoint& ep);
		virtual   ~server();
		void      update(int ms);
		void      close();
		uint16_t  get_port() const;

        bool output(const char* buf, size_t len);
        bool input(std::string& buf);

    private:
        bool is_closed() const;
        void event_accept(bee::net::socket::fd_t fd);

	private:
        bee::net::endpoint  endpoint_;
        session*            session_;
	};
}}
